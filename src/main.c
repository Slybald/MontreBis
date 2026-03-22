/* Connected Watch — Main Init — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "app_events.h"
#include "buttons.h"
#include "rtc_time.h"
#include "sensors.h"
#include "controller.h"
#include "ui/ui.h"
#include "ble_service.h"
#include "storage.h"

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

/* Kernel objects — owned by main, extern'd in app_events.h */
K_MSGQ_DEFINE(event_bus, sizeof(struct event_msg), 16, 4);
K_MSGQ_DEFINE(raw_input_q, sizeof(enum raw_input), 16, 4);
K_SEM_DEFINE(sem_sensor_start, 0, 1);
K_SEM_DEFINE(sem_display_ready, 0, 1);
K_SEM_DEFINE(sem_display_wake, 0, 1);
K_MUTEX_DEFINE(mtx_sensors);

struct sensor_snapshot shared_sensors;

static void on_ble_time_update(uint32_t timestamp)
{
    event_post_val(EVT_BLE_TIME_SYNC, timestamp);
}

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_touch))
static const struct device *const touch_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_touch));

static struct {
    int32_t x;
    int32_t y;
    bool pressed;
} touch_diag_state = {
    .x = -1,
    .y = -1,
    .pressed = false,
};

static void touch_diag_callback(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    switch (evt->code) {
    case INPUT_ABS_X:
        touch_diag_state.x = evt->value;
        break;
    case INPUT_ABS_Y:
        touch_diag_state.y = evt->value;
        break;
    case INPUT_BTN_TOUCH:
        touch_diag_state.pressed = (evt->value != 0);
        break;
    default:
        break;
    }

    if (evt->sync) {
        LOG_INF("TOUCH %s x=%ld y=%ld",
                touch_diag_state.pressed ? "PRESS" : "RELEASE",
                (long)touch_diag_state.x,
                (long)touch_diag_state.y);
    }
}
INPUT_CALLBACK_DEFINE(touch_dev, touch_diag_callback, NULL);
#endif

int main(void)
{
    LOG_INF("========================================");
    LOG_INF("Connected Watch — Event-Driven Architecture");
    LOG_INF("========================================");

    k_msleep(100);

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display not ready");
        return -1;
    }
    display_blanking_off(display_dev);

    ui_init();
    LOG_INF("UI initialized");

    if (init_buttons() != 0) {
        LOG_ERR("Button init failed");
        return -1;
    }

    storage_init();
    ui_set_theme_index(storage_load_theme());

    rtc_time_init();
    sensors_init();
    controller_set_display(display_dev);

    int err = ble_init(on_ble_time_update);
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
    } else {
        LOG_INF("BLE initialized");
    }

    k_sem_give(&sem_display_ready);

    LOG_INF("System operational");
    k_sleep(K_FOREVER);
    return 0;
}
