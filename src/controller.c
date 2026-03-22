/* Controller FSM — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <math.h>

#include "app_events.h"
#include "ui/ui.h"
#include "ble_service.h"
#include "rtc_time.h"
#include "storage.h"
#include "controller.h"

LOG_MODULE_REGISTER(controller, CONFIG_LOG_DEFAULT_LEVEL);

#define SENSOR_READ_INTERVAL_ACTIVE_MS 250
#define SENSOR_READ_INTERVAL_IDLE_MS   2000
#define TIME_UPDATE_INTERVAL_MS        1000
#define INACTIVITY_SLOWDOWN_SEC        30
#define INACTIVITY_SLEEP_SEC           120

#define LANDSCAPE_THRESHOLD       5.0f
#define ROTATION_SAMPLES_REQUIRED 2

#define DEBOUNCE_MS 200

/* Display device (set by main via controller_set_display) */
static const struct device *display_dev;

enum power_profile {
    POWER_PROFILE_ACTIVE = 0,
    POWER_PROFILE_IDLE,
    POWER_PROFILE_SLEEP,
};

/* Auto-rotation state (controller thread only) */
static enum display_orientation rot_current = DISPLAY_ORIENTATION_ROTATED_90;
static enum display_orientation rot_target  = DISPLAY_ORIENTATION_ROTATED_90;
static uint8_t rot_sample_count;
static enum power_profile current_power_profile;
static uint32_t current_sensor_interval_ms;

/* Shared state (inter-thread) — defined here, extern'd in app_events.h */
atomic_t ui_action_flags;
atomic_t sensor_display_dirty;
atomic_t display_sleeping;
atomic_t shared_step_count;
atomic_t shared_heading_centideg;
atomic_t touch_activity_flag;
static int64_t last_activity_ms;

/* Pedometer state */
static uint32_t step_count;
static uint32_t step_count_last_saved;
static bool  step_rising;
static float filtered_accel_mag;
#define STEP_THRESHOLD_HIGH  14.0f
#define STEP_THRESHOLD_LOW   8.0f
#define STEP_SAVE_INTERVAL   100
#define ACCEL_MAG_FILTER_ALPHA 0.3f

#define RAD2DEG 57.2957795f

/* Forward declarations */
static void input_mgr_entry(void *, void *, void *);
static void controller_entry(void *, void *, void *);
static void timer_sensors_expiry(struct k_timer *t);
static void timer_time_expiry(struct k_timer *t);
static void set_sensor_timer_period(uint32_t interval_ms);
static void enter_sleep_mode(void);
static void wake_from_sleep(void);

/* Thread definitions */
K_THREAD_DEFINE(input_mgr_tid, 1536, input_mgr_entry,
                NULL, NULL, NULL, 3, 0, 0);

K_THREAD_DEFINE(controller_tid, 4096, controller_entry,
                NULL, NULL, NULL, 5, 0, 0);

/* Timer definitions */
K_TIMER_DEFINE(timer_sensors, timer_sensors_expiry, NULL);
K_TIMER_DEFINE(timer_time,    timer_time_expiry,    NULL);

static void timer_sensors_expiry(struct k_timer *t)
{
    ARG_UNUSED(t);
    raw_input_post(RAW_TIMER_SENSORS);
}

static void timer_time_expiry(struct k_timer *t)
{
    ARG_UNUSED(t);
    raw_input_post(RAW_TIMER_TIME);
}

static void event_post_log(enum event_type type)
{
    if (event_post(type) == -EBUSY) {
        LOG_WRN("Event bus full, dropped type=%d", type);
    }
}

void controller_set_display(const struct device *dev)
{
    display_dev = dev;
    current_power_profile = POWER_PROFILE_ACTIVE;
    current_sensor_interval_ms = SENSOR_READ_INTERVAL_ACTIVE_MS;
    last_activity_ms = k_uptime_get();
    step_count = storage_load_steps();
    step_count_last_saved = step_count;
    atomic_set(&ui_action_flags, 0);
    atomic_set(&sensor_display_dirty, 0);
    atomic_set(&display_sleeping, 0);
    atomic_set(&shared_step_count, step_count);

    k_timer_start(&timer_sensors,
                  K_MSEC(SENSOR_READ_INTERVAL_ACTIVE_MS),
                  K_MSEC(SENSOR_READ_INTERVAL_ACTIVE_MS));

    k_timer_start(&timer_time,
                  K_MSEC(TIME_UPDATE_INTERVAL_MS),
                  K_MSEC(TIME_UPDATE_INTERVAL_MS));
}

static void set_sensor_timer_period(uint32_t interval_ms)
{
    if (current_sensor_interval_ms == interval_ms) {
        return;
    }

    current_sensor_interval_ms = interval_ms;
    k_timer_start(&timer_sensors, K_MSEC(interval_ms), K_MSEC(interval_ms));
}

static void enter_sleep_mode(void)
{
    LOG_INF("[FSM] IDLE → SLEEP (inactivity %ds)", INACTIVITY_SLEEP_SEC);

    current_power_profile = POWER_PROFILE_SLEEP;
    k_sem_reset(&sem_display_wake);
    k_timer_stop(&timer_sensors);
    k_timer_stop(&timer_time);
    atomic_or(&ui_action_flags, UI_ACT_ENTER_SLEEP);

    if (display_dev) {
        display_blanking_on(display_dev);
    }
}

static void wake_from_sleep(void)
{
    current_power_profile = POWER_PROFILE_ACTIVE;
    current_sensor_interval_ms = SENSOR_READ_INTERVAL_ACTIVE_MS;
    last_activity_ms = k_uptime_get();

    if (display_dev) {
        display_blanking_off(display_dev);
    }

    atomic_set(&display_sleeping, 0);
    atomic_or(&ui_action_flags, UI_ACT_EXIT_SLEEP);
    k_sem_give(&sem_display_wake);

    k_timer_start(&timer_sensors,
                  K_MSEC(SENSOR_READ_INTERVAL_ACTIVE_MS),
                  K_MSEC(SENSOR_READ_INTERVAL_ACTIVE_MS));
    k_timer_start(&timer_time,
                  K_MSEC(TIME_UPDATE_INTERVAL_MS),
                  K_MSEC(TIME_UPDATE_INTERVAL_MS));

    k_sem_give(&sem_sensor_start);
}

/* ------------------------------------------------------------------ */
/* INPUT_MGR Thread (Priority 3) — Debounce + Translate               */
/* ------------------------------------------------------------------ */

static int64_t last_btn_ms[4];

static void input_mgr_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    enum raw_input raw;
    LOG_INF("[INPUT_MGR] Thread started (P3)");

    while (1) {
        k_msgq_get(&raw_input_q, &raw, K_FOREVER);

        if (raw <= RAW_BTN3) {
            int64_t now = k_uptime_get();
            if ((now - last_btn_ms[raw]) < DEBOUNCE_MS) {
                continue;
            }
            last_btn_ms[raw] = now;
        }

        switch (raw) {
        case RAW_BTN0: event_post_log(EVT_BTN_SWITCH_SCREEN); break;
        case RAW_BTN1: event_post_log(EVT_BTN_CYCLE_THEME); break;
        case RAW_BTN2: event_post_log(EVT_BTN_BRIGHTNESS); break;
        case RAW_BTN3: event_post_log(EVT_BTN_STATUS_POPUP); break;
        case RAW_TIMER_SENSORS: event_post_log(EVT_TICK_SENSORS); break;
        case RAW_TIMER_TIME: event_post_log(EVT_TICK_TIME); break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Controller Helpers                                                  */
/* ------------------------------------------------------------------ */

static void check_auto_rotation(const struct sensor_snapshot *snap)
{
    if (!(snap->valid_flags & SNAP_VALID_MOTION)) return;

    float axf    = snap->accel[0];
    float ayf    = snap->accel[1];
    float abs_ax = fabsf(axf);
    float abs_ay = fabsf(ayf);

    enum display_orientation new_tgt = rot_current;

    if (abs_ay > abs_ax) {
        if (ayf > LANDSCAPE_THRESHOLD) {
            new_tgt = DISPLAY_ORIENTATION_ROTATED_270;
        } else if (ayf < -LANDSCAPE_THRESHOLD) {
            new_tgt = DISPLAY_ORIENTATION_ROTATED_90;
        }
    }

    if (new_tgt != rot_target) {
        rot_target = new_tgt;
        rot_sample_count = 1;
    } else if (rot_target == rot_current) {
        rot_sample_count = 0;
    } else {
        rot_sample_count++;
    }

    if (rot_target != rot_current &&
        rot_sample_count >= ROTATION_SAMPLES_REQUIRED) {

        LOG_INF("Auto-rotation → %s",
                rot_target == DISPLAY_ORIENTATION_ROTATED_90 ? "90°" : "270°");
        rot_sample_count = 0;

        int err = display_set_orientation(display_dev, rot_target);
        if (err == 0) {
            rot_current = rot_target;
            atomic_or(&ui_action_flags, UI_ACT_ROTATION_CHANGE);
        } else {
            LOG_ERR("display_set_orientation failed: %d", err);
        }
    }
}

static void process_sensor_data(void)
{
    struct sensor_snapshot snap;

    k_mutex_lock(&mtx_sensors, K_FOREVER);
    snap = shared_sensors;
    k_mutex_unlock(&mtx_sensors);

    if (!snap.valid_flags) return;

    if (snap.valid_flags & SNAP_VALID_ENV) {
        ble_update_env_data((int16_t)(snap.temperature * 100.0f),
                            (uint16_t)(snap.humidity * 100.0f));
    }
    if (snap.valid_flags & SNAP_VALID_PRESS) {
        ble_update_pressure((uint32_t)(snap.pressure * 1000.0f));
    }
    if (snap.valid_flags & SNAP_VALID_MOTION) {
        ble_update_motion_data(
            (int16_t)(snap.accel[0] * 100.0f), (int16_t)(snap.accel[1] * 100.0f),
            (int16_t)(snap.accel[2] * 100.0f),
            (int16_t)(snap.gyro[0] * 100.0f),  (int16_t)(snap.gyro[1] * 100.0f),
            (int16_t)(snap.gyro[2] * 100.0f));
    }

    /* Compass heading from magnetometer — tilt-compensated if accel available */
    if (snap.valid_flags & SNAP_VALID_MAG) {
        ble_update_magnetometer(
            (int16_t)(snap.magn[0] * 1000.0f),
            (int16_t)(snap.magn[1] * 1000.0f),
            (int16_t)(snap.magn[2] * 1000.0f));

        float heading;

        if (snap.valid_flags & SNAP_VALID_MOTION) {
            float ax = snap.accel[0], ay = snap.accel[1], az = snap.accel[2];
            float mx = snap.magn[0],  my = snap.magn[1],  mz = snap.magn[2];
            float g = sqrtf(ax * ax + ay * ay + az * az);

            if (g > 0.5f) {
                float pitch = asinf(-ax / g);
                float roll  = atan2f(ay, az);
                float cos_p = cosf(pitch), sin_p = sinf(pitch);
                float cos_r = cosf(roll),  sin_r = sinf(roll);

                float bx = mx * cos_p + mz * sin_p;
                float by = mx * sin_r * sin_p + my * cos_r
                           - mz * sin_r * cos_p;
                heading = atan2f(-by, bx) * RAD2DEG;
            } else {
                heading = atan2f(snap.magn[1], snap.magn[0]) * RAD2DEG;
            }
        } else {
            heading = atan2f(snap.magn[1], snap.magn[0]) * RAD2DEG;
        }

        if (heading < 0.0f) heading += 360.0f;
        ble_update_compass((int16_t)(heading * 100.0f));
        atomic_set(&shared_heading_centideg, (atomic_val_t)(int)(heading * 100.0f));
    }

    /* Pedometer: peak detection on low-pass filtered acceleration magnitude */
    if (snap.valid_flags & SNAP_VALID_MOTION) {
        float mag = sqrtf(snap.accel[0] * snap.accel[0] +
                          snap.accel[1] * snap.accel[1] +
                          snap.accel[2] * snap.accel[2]);

        filtered_accel_mag = ACCEL_MAG_FILTER_ALPHA * mag
                           + (1.0f - ACCEL_MAG_FILTER_ALPHA) * filtered_accel_mag;

        if (!step_rising && filtered_accel_mag > STEP_THRESHOLD_HIGH) {
            step_rising = true;
        } else if (step_rising && filtered_accel_mag < STEP_THRESHOLD_LOW) {
            step_rising = false;
            step_count++;
            ble_update_steps(step_count);
            if ((step_count - step_count_last_saved) >= STEP_SAVE_INTERVAL) {
                storage_save_steps(step_count);
                step_count_last_saved = step_count;
            }
        }
        atomic_set(&shared_step_count, (atomic_val_t)step_count);
    }

    check_auto_rotation(&snap);

    atomic_set(&sensor_display_dirty, 1);
}

static void handle_button_event(const struct event_msg *msg)
{
    switch (msg->type) {
    case EVT_BTN_SWITCH_SCREEN:
        atomic_or(&ui_action_flags, UI_ACT_SWITCH_SCREEN);
        break;
    case EVT_BTN_CYCLE_THEME:
        atomic_or(&ui_action_flags, UI_ACT_CYCLE_THEME);
        break;
    case EVT_BTN_BRIGHTNESS:
        atomic_or(&ui_action_flags, UI_ACT_BRIGHTNESS);
        break;
    case EVT_BTN_STATUS_POPUP:
        atomic_or(&ui_action_flags, UI_ACT_STATUS_POPUP);
        break;
    default:
        return;
    }
    last_activity_ms = k_uptime_get();
    if (current_power_profile != POWER_PROFILE_ACTIVE) {
        current_power_profile = POWER_PROFILE_ACTIVE;
        set_sensor_timer_period(SENSOR_READ_INTERVAL_ACTIVE_MS);
    }
}

/* ------------------------------------------------------------------ */
/* CONTROLLER Thread (Priority 5) — FSM                               */
/* ------------------------------------------------------------------ */

static void controller_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    enum app_state state = STATE_INIT;
    struct event_msg msg;

    LOG_INF("[CONTROLLER] Thread started (P5) — FSM ready");

    while (1) {
        k_msgq_get(&event_bus, &msg, K_FOREVER);

        switch (state) {

        case STATE_INIT:
            LOG_INF("[FSM] INIT → IDLE");
            last_activity_ms = k_uptime_get();
            state = STATE_IDLE;
            __attribute__((fallthrough));

        case STATE_IDLE:
            switch (msg.type) {

            case EVT_TICK_SENSORS:
                state = STATE_ACTIVE;
                k_sem_give(&sem_sensor_start);
                break;

            case EVT_SENSOR_DATA_READY:
                process_sensor_data();
                state = STATE_IDLE;
                break;

            case EVT_TICK_TIME:
            {
                if (atomic_cas(&touch_activity_flag, 1, 0)) {
                    last_activity_ms = k_uptime_get();
                }
                int64_t inactive_ms = k_uptime_get() - last_activity_ms;

                if (inactive_ms >= (INACTIVITY_SLEEP_SEC * 1000LL)) {
                    state = STATE_SLEEP;
                    enter_sleep_mode();
                } else if (inactive_ms >= (INACTIVITY_SLOWDOWN_SEC * 1000LL)) {
                    if (current_power_profile != POWER_PROFILE_IDLE) {
                        current_power_profile = POWER_PROFILE_IDLE;
                        set_sensor_timer_period(SENSOR_READ_INTERVAL_IDLE_MS);
                    }
                } else if (current_power_profile != POWER_PROFILE_ACTIVE) {
                    current_power_profile = POWER_PROFILE_ACTIVE;
                    set_sensor_timer_period(SENSOR_READ_INTERVAL_ACTIVE_MS);
                }
                break;
            }

            case EVT_BLE_TIME_SYNC:
                time_sync_from_ble(msg.data.timestamp);
                break;

            default:
                handle_button_event(&msg);
                break;
            }
            break;

        case STATE_ACTIVE:
            switch (msg.type) {

            case EVT_SENSOR_DATA_READY:
                process_sensor_data();
                state = STATE_IDLE;
                break;

            case EVT_BLE_TIME_SYNC:
                time_sync_from_ble(msg.data.timestamp);
                break;

            case EVT_TICK_TIME:
            {
                if (atomic_cas(&touch_activity_flag, 1, 0)) {
                    last_activity_ms = k_uptime_get();
                }
                int64_t inactive_ms = k_uptime_get() - last_activity_ms;

                if (inactive_ms >= (INACTIVITY_SLOWDOWN_SEC * 1000LL) &&
                    current_power_profile == POWER_PROFILE_ACTIVE) {
                    current_power_profile = POWER_PROFILE_IDLE;
                    set_sensor_timer_period(SENSOR_READ_INTERVAL_IDLE_MS);
                }
                break;
            }

            default:
                handle_button_event(&msg);
                break;
            }
            break;

        case STATE_SLEEP:
            if (msg.type >= EVT_BTN_SWITCH_SCREEN &&
                msg.type <= EVT_BTN_STATUS_POPUP) {

                LOG_INF("[FSM] SLEEP → IDLE (button wake)");
                state = STATE_IDLE;
                wake_from_sleep();
            } else if (msg.type == EVT_BLE_TIME_SYNC) {
                time_sync_from_ble(msg.data.timestamp);
            }
            break;

        } /* switch (state) */
    } /* while (1) */
}
