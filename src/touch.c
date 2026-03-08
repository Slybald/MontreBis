/* FT6206 Touch Driver — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

#include "touch.h"

LOG_MODULE_REGISTER(touch, CONFIG_LOG_DEFAULT_LEVEL);

#define FT6206_I2C_ADDR      0x38U
#define FT6206_REG_TD_STATUS  0x02U
#define FT6206_REG_P1_XH     0x03U
#define FT6206_TOUCH_RES_X   240
#define FT6206_TOUCH_RES_Y   320
#define DISP_LANDSCAPE_W     320
#define DISP_LANDSCAPE_H     240

#define TOUCH_SWAP_XY   1
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  1

static const struct device *i2c_touch_dev;

static void ft6206_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint8_t buf[5];

    (void)indev;

    if (!i2c_touch_dev || !device_is_ready(i2c_touch_dev)) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    if (i2c_burst_read(i2c_touch_dev, FT6206_I2C_ADDR,
                       FT6206_REG_TD_STATUS, buf, sizeof(buf)) != 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    if ((buf[0] & 0x0FU) == 0U) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
        return;
    }

    uint16_t x_raw = (uint16_t)((buf[1] & 0x3FU) << 8) | buf[2];
    uint16_t y_raw = (uint16_t)((buf[3] & 0x3FU) << 8) | buf[4];

    if (x_raw > FT6206_TOUCH_RES_X - 1) x_raw = FT6206_TOUCH_RES_X - 1;
    if (y_raw > FT6206_TOUCH_RES_Y - 1) y_raw = FT6206_TOUCH_RES_Y - 1;

    int32_t x_out, y_out;
#if TOUCH_SWAP_XY
    x_out = (int32_t)y_raw;
    y_out = (int32_t)x_raw;
#else
    x_out = (int32_t)x_raw;
    y_out = (int32_t)y_raw;
#endif
#if TOUCH_INVERT_X
    x_out = DISP_LANDSCAPE_W - 1 - x_out;
#endif
#if TOUCH_INVERT_Y
    y_out = DISP_LANDSCAPE_H - 1 - y_out;
#endif

    if (x_out < 0) x_out = 0;
    if (x_out >= DISP_LANDSCAPE_W) x_out = DISP_LANDSCAPE_W - 1;
    if (y_out < 0) y_out = 0;
    if (y_out >= DISP_LANDSCAPE_H) y_out = DISP_LANDSCAPE_H - 1;

    data->point.x = x_out;
    data->point.y = y_out;
    data->state = LV_INDEV_STATE_PRESSED;
    data->continue_reading = false;
}

int init_touch(void)
{
    i2c_touch_dev = DEVICE_DT_GET(DT_BUS(DT_NODELABEL(hts221_x_nucleo_iks01a3)));
    if (!device_is_ready(i2c_touch_dev)) {
        LOG_WRN("Touch I2C bus not ready (FT6206 on arduino_i2c)");
        return -1;
    }

    lv_indev_t *indev = lv_indev_create();
    if (!indev) {
        LOG_ERR("Failed to create FT6206 indev");
        return -1;
    }
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, ft6206_read);
    lv_indev_set_display(indev, lv_display_get_default());

    LOG_INF("FT6206 touch registered (Landscape 320x240)");
    return 0;
}
