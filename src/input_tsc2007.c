/* SPDX-License-Identifier: Apache-2.0 */

#define DT_DRV_COMPAT ti_tsc2007

#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_touch.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tsc2007, CONFIG_INPUT_LOG_LEVEL);

#define TSC2007_FUNC_TEMP0 0x00U
#define TSC2007_FUNC_X 0x0CU
#define TSC2007_FUNC_Y 0x0DU
#define TSC2007_FUNC_Z1 0x0EU
#define TSC2007_FUNC_Z2 0x0FU

#define TSC2007_POWERDOWN_IRQON 0x00U
#define TSC2007_ADON_IRQOFF 0x01U

#define TSC2007_ADC_12BIT 0x00U

#define TSC2007_CONVERSION_DELAY_US 500U
#define TSC2007_ADDR_MIN 0x48U
#define TSC2007_ADDR_MAX 0x4BU
#define TSC2007_STABILITY_DELTA 100U

struct tsc2007_config {
	struct input_touchscreen_common_config common;
	struct i2c_dt_spec bus;
	int raw_x_min;
	int raw_y_min;
	uint16_t raw_x_max;
	uint16_t raw_y_max;
	uint16_t pressure_min;
	uint16_t poll_interval_ms;
};

struct tsc2007_data {
	const struct device *dev;
	struct k_work_delayable poll_work;
	bool pressed;
	int last_error;
};

INPUT_TOUCH_STRUCT_CHECK(struct tsc2007_config);

static uint32_t tsc2007_abs_diff(uint16_t a, uint16_t b)
{
	return (a >= b) ? (uint32_t)(a - b) : (uint32_t)(b - a);
}

static void tsc2007_scan_bus(const struct tsc2007_config *config)
{
	uint8_t command = (uint8_t)((TSC2007_FUNC_TEMP0 << 4) |
				    (TSC2007_POWERDOWN_IRQON << 2) |
				    (TSC2007_ADC_12BIT << 1));

	for (uint8_t addr = TSC2007_ADDR_MIN; addr <= TSC2007_ADDR_MAX; addr++) {
		int ret = i2c_write(config->bus.bus, &command, sizeof(command), addr);

		if (ret == 0) {
			LOG_INF("TSC2007 candidate detected at 0x%02x", addr);
		}
	}
}

static void tsc2007_report_pos(const struct device *dev, uint32_t x, uint32_t y)
{
	const struct input_touchscreen_common_config *cfg = dev->config;
	const uint32_t reported_x_code = cfg->swapped_x_y ? INPUT_ABS_Y : INPUT_ABS_X;
	const uint32_t reported_y_code = cfg->swapped_x_y ? INPUT_ABS_X : INPUT_ABS_Y;
	const uint32_t reported_x = cfg->inverted_x ? cfg->screen_width - x : x;
	const uint32_t reported_y = cfg->inverted_y ? cfg->screen_height - y : y;

	input_report_abs(dev, reported_x_code, reported_x, false, K_FOREVER);
	input_report_abs(dev, reported_y_code, reported_y, false, K_FOREVER);
}

static uint16_t tsc2007_scale_axis(uint16_t raw, int raw_min, uint16_t raw_max, uint32_t size)
{
	if (size == 0U || raw_max <= raw_min) {
		return raw;
	}

	if (raw <= raw_min) {
		return 0U;
	}

	if (raw >= raw_max) {
		return (uint16_t)(size - 1U);
	}

	return (uint16_t)(((uint32_t)(raw - raw_min) * (size - 1U)) /
			  (uint32_t)(raw_max - raw_min));
}

static int tsc2007_read_sample(const struct tsc2007_config *config, uint8_t func, uint8_t power,
			       uint16_t *value)
{
	uint8_t command = (uint8_t)((func << 4) | (power << 2) | (TSC2007_ADC_12BIT << 1));
	uint8_t rx_buf[2];
	int ret;

	ret = i2c_write_dt(&config->bus, &command, sizeof(command));
	if (ret < 0) {
		return ret;
	}

	k_usleep(TSC2007_CONVERSION_DELAY_US);

	ret = i2c_read_dt(&config->bus, rx_buf, sizeof(rx_buf));
	if (ret < 0) {
		return ret;
	}

	*value = ((uint16_t)rx_buf[0] << 4) | (rx_buf[1] >> 4);

	return 0;
}

static void tsc2007_report_release(const struct device *dev, struct tsc2007_data *data)
{
	if (!data->pressed) {
		return;
	}

	input_report_key(dev, INPUT_BTN_TOUCH, 0, true, K_FOREVER);
	data->pressed = false;
}

static int tsc2007_process(const struct device *dev)
{
	const struct tsc2007_config *config = dev->config;
	struct tsc2007_data *data = dev->data;
	uint16_t z1;
	uint16_t z2;
	uint16_t raw_x1;
	uint16_t raw_y1;
	uint16_t raw_x2;
	uint16_t raw_y2;
	uint16_t x;
	uint16_t y;
	int ret;

	ret = tsc2007_read_sample(config, TSC2007_FUNC_Z1, TSC2007_ADON_IRQOFF, &z1);
	if (ret < 0) {
		return ret;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_Z2, TSC2007_ADON_IRQOFF, &z2);
	if (ret < 0) {
		return ret;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_X, TSC2007_ADON_IRQOFF, &raw_x1);
	if (ret < 0) {
		return ret;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_Y, TSC2007_ADON_IRQOFF, &raw_y1);
	if (ret < 0) {
		return ret;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_X, TSC2007_ADON_IRQOFF, &raw_x2);
	if (ret < 0) {
		return ret;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_Y, TSC2007_ADON_IRQOFF, &raw_y2);
	if (ret < 0) {
		return ret;
	}

	(void)tsc2007_read_sample(config, TSC2007_FUNC_TEMP0, TSC2007_POWERDOWN_IRQON, &z2);

	if (z1 <= config->pressure_min || z2 == 0U) {
		tsc2007_report_release(dev, data);
		return 0;
	}

	if (tsc2007_abs_diff(raw_x1, raw_x2) > TSC2007_STABILITY_DELTA ||
	    tsc2007_abs_diff(raw_y1, raw_y2) > TSC2007_STABILITY_DELTA ||
	    raw_x1 == UINT16_MAX || raw_y1 == UINT16_MAX || raw_x1 == 4095U || raw_y1 == 4095U) {
		tsc2007_report_release(dev, data);
		return 0;
	}

	x = tsc2007_scale_axis(raw_x1, config->raw_x_min, config->raw_x_max,
			       config->common.screen_width);
	y = tsc2007_scale_axis(raw_y1, config->raw_y_min, config->raw_y_max,
			       config->common.screen_height);

	tsc2007_report_pos(dev, x, y);
	input_report_key(dev, INPUT_BTN_TOUCH, 1, true, K_FOREVER);
	data->pressed = true;

	return 0;
}

static void tsc2007_poll_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
	struct tsc2007_data *data = CONTAINER_OF(dwork, struct tsc2007_data, poll_work);
	const struct device *dev = data->dev;
	const struct tsc2007_config *config = dev->config;
	int ret;

	ret = tsc2007_process(dev);
	if (ret < 0) {
		if (ret != data->last_error) {
			LOG_ERR("TSC2007 read failed: %d", ret);
			data->last_error = ret;
		}
		tsc2007_report_release(dev, data);
	} else {
		data->last_error = 0;
	}

	k_work_reschedule(&data->poll_work, K_MSEC(config->poll_interval_ms));
}

static int tsc2007_init(const struct device *dev)
{
	const struct tsc2007_config *config = dev->config;
	struct tsc2007_data *data = dev->data;
	uint16_t tmp;
	int ret;

	if (!i2c_is_ready_dt(&config->bus)) {
		LOG_ERR("I2C bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	}

	ret = tsc2007_read_sample(config, TSC2007_FUNC_TEMP0, TSC2007_POWERDOWN_IRQON, &tmp);
	if (ret < 0) {
		LOG_ERR("TSC2007 setup failed: %d", ret);
		tsc2007_scan_bus(config);
		return ret;
	}

	data->dev = dev;
	data->pressed = false;
	data->last_error = 0;

	k_work_init_delayable(&data->poll_work, tsc2007_poll_work_handler);
	k_work_schedule(&data->poll_work, K_NO_WAIT);

	LOG_INF("TSC2007 touchscreen ready on %s", config->bus.bus->name);

	return 0;
}

#define TSC2007_DEFINE(inst)                                                                    \
	static const struct tsc2007_config tsc2007_config_##inst = {                           \
		.common = INPUT_TOUCH_DT_INST_COMMON_CONFIG_INIT(inst),                        \
		.bus = I2C_DT_SPEC_INST_GET(inst),                                             \
		.raw_x_min = DT_INST_PROP(inst, raw_x_min),                                    \
		.raw_y_min = DT_INST_PROP(inst, raw_y_min),                                    \
		.raw_x_max = DT_INST_PROP(inst, raw_x_max),                                    \
		.raw_y_max = DT_INST_PROP(inst, raw_y_max),                                    \
		.pressure_min = DT_INST_PROP(inst, pressure_min),                              \
		.poll_interval_ms = DT_INST_PROP(inst, poll_interval_ms),                      \
	};                                                                                      \
	static struct tsc2007_data tsc2007_data_##inst;                                        \
	DEVICE_DT_INST_DEFINE(inst, tsc2007_init, NULL, &tsc2007_data_##inst,                  \
			      &tsc2007_config_##inst, POST_KERNEL,                             \
			      CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(TSC2007_DEFINE)
