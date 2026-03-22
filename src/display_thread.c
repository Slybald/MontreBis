/* Display Thread — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <math.h>
#include <time.h>

#include "app_events.h"
#include "ui/ui.h"
#include "ble_service.h"
#include "rtc_time.h"
#include "storage.h"
#include "display_thread.h"

LOG_MODULE_REGISTER(display, CONFIG_LOG_DEFAULT_LEVEL);

#define LVGL_TASK_INTERVAL_MS 10
#define STEP_GOAL            10000U

static void display_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_sem_take(&sem_display_ready, K_FOREVER);

    LOG_INF("[DISPLAY] Thread started (P9)");

    static int64_t last_time_s = -1;
    static int last_calendar_day = -1;

    while (1) {
        if (atomic_get(&display_sleeping) != 0) {
            k_sem_take(&sem_display_wake, K_FOREVER);
            last_time_s = -1;
            last_calendar_day = -1;
            continue;
        }

        /* 1. Process UI actions from CONTROLLER */
        atomic_val_t actions = atomic_clear(&ui_action_flags);
        bool entering_sleep = (actions & UI_ACT_ENTER_SLEEP) != 0U;

        if (actions & UI_ACT_SWITCH_SCREEN)   ui_switch_screen();
        if (actions & UI_ACT_CYCLE_THEME) {
            ui_cycle_theme_color();
            storage_save_theme(ui_get_theme_index());
        }
        if (actions & UI_ACT_BRIGHTNESS)      ui_toggle_brightness();
        if (actions & UI_ACT_STATUS_POPUP)    ui_toggle_status_popup();
        if (actions & UI_ACT_EXIT_SLEEP)      ui_hide_sleep_overlay();
        if (actions & UI_ACT_ROTATION_CHANGE) {
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);
        }
        if (entering_sleep) {
            ui_show_sleep_overlay();
            lv_refr_now(NULL);
            atomic_set(&display_sleeping, 1);
            continue;
        }

        /* 2. Update sensor display if new data available */
        if (atomic_cas(&sensor_display_dirty, 1, 0)) {
            struct sensor_snapshot snap;
            k_mutex_lock(&mtx_sensors, K_FOREVER);
            snap = shared_sensors;
            k_mutex_unlock(&mtx_sensors);

            if (snap.valid_flags) {
                ui_update_temp_humidity(snap.temperature, snap.humidity);
                ui_update_pressure(snap.pressure);
                ui_update_accel(snap.accel[0], snap.accel[1], snap.accel[2]);
                ui_update_gyro(snap.gyro[0], snap.gyro[1], snap.gyro[2]);
                ui_update_magnetometer(snap.magn[0], snap.magn[1],
                                       snap.magn[2]);

                if (snap.valid_flags & SNAP_VALID_MAG) {
                    const float rad2deg = 57.2957795f;
                    float heading = atan2f(snap.magn[1], snap.magn[0]) * rad2deg;
                    if (heading < 0.0f) {
                        heading += 360.0f;
                    }
                    ui_update_compass(heading);
                }

                ui_update_steps((uint32_t)atomic_get(&shared_step_count), STEP_GOAL);
            }
        }

        /* 3. Update time display (once per second) */
        int64_t now_s = k_uptime_get() / 1000;
        if (now_s != last_time_s) {
            last_time_s = now_s;

            ui_update_uptime((uint32_t)now_s);
            ui_set_ble_status(ble_is_connected());

            uint32_t current_timestamp;
            if (time_get_unix(&current_timestamp)) {
                time_t current = (time_t)current_timestamp;
                struct tm t;
                gmtime_r(&current, &t);

                int year  = t.tm_year + 1900;
                int month = t.tm_mon + 1;
                int day   = t.tm_mday;

                ui_update_datetime(year, month, day,
                                   t.tm_hour, t.tm_min, t.tm_sec);

                if (day != last_calendar_day) {
                    last_calendar_day = day;
                    ui_update_calendar(year, month, day);
                }
            }
        }

        if (actions & UI_ACT_EXIT_SLEEP) {
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);
        }

        /* 4. LVGL task handler */
        lv_task_handler();

        /* 5. Yield CPU — 10 ms (100 Hz refresh) */
        k_msleep(LVGL_TASK_INTERVAL_MS);
    }
}

K_THREAD_DEFINE(display_tid, 10240, display_entry,
                NULL, NULL, NULL, 9, 0, 0);
