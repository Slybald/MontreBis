/* Button GPIO Driver — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "app_events.h"
#include "buttons.h"

LOG_MODULE_REGISTER(buttons, CONFIG_LOG_DEFAULT_LEVEL);

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
#endif
#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
#endif
#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
#endif
#if DT_NODE_HAS_STATUS(SW3_NODE, okay)
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);
#endif

static struct gpio_callback button_cb_data[4];

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
static void button0_isr(const struct device *d, struct gpio_callback *cb,
                         uint32_t pins)
{
    ARG_UNUSED(d); ARG_UNUSED(cb); ARG_UNUSED(pins);
    raw_input_post(RAW_BTN0);
}
#endif

#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
static void button1_isr(const struct device *d, struct gpio_callback *cb,
                         uint32_t pins)
{
    ARG_UNUSED(d); ARG_UNUSED(cb); ARG_UNUSED(pins);
    raw_input_post(RAW_BTN1);
}
#endif

#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
static void button2_isr(const struct device *d, struct gpio_callback *cb,
                         uint32_t pins)
{
    ARG_UNUSED(d); ARG_UNUSED(cb); ARG_UNUSED(pins);
    raw_input_post(RAW_BTN2);
}
#endif

#if DT_NODE_HAS_STATUS(SW3_NODE, okay)
static void button3_isr(const struct device *d, struct gpio_callback *cb,
                         uint32_t pins)
{
    ARG_UNUSED(d); ARG_UNUSED(cb); ARG_UNUSED(pins);
    raw_input_post(RAW_BTN3);
}
#endif

int init_buttons(void)
{
    int ret = 0;

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
    if (gpio_is_ready_dt(&button0)) {
        ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
        if (ret == 0) ret = gpio_pin_interrupt_configure_dt(
                                &button0, GPIO_INT_EDGE_TO_ACTIVE);
        if (ret == 0) {
            gpio_init_callback(&button_cb_data[0], button0_isr,
                               BIT(button0.pin));
            ret = gpio_add_callback(button0.port, &button_cb_data[0]);
        }
        if (ret != 0) {
            LOG_ERR("SW0 init failed: %d", ret);
            return -1;
        }
        LOG_INF("SW0 configured (Switch Screen)");
    }
#endif
#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
    if (gpio_is_ready_dt(&button1)) {
        ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
        if (ret == 0) ret = gpio_pin_interrupt_configure_dt(
                                &button1, GPIO_INT_EDGE_TO_ACTIVE);
        if (ret == 0) {
            gpio_init_callback(&button_cb_data[1], button1_isr,
                               BIT(button1.pin));
            ret = gpio_add_callback(button1.port, &button_cb_data[1]);
        }
        if (ret != 0) {
            LOG_ERR("SW1 init failed: %d", ret);
            return -1;
        }
        LOG_INF("SW1 configured (Cycle Theme)");
    }
#endif
#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
    if (gpio_is_ready_dt(&button2)) {
        ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
        if (ret == 0) ret = gpio_pin_interrupt_configure_dt(
                                &button2, GPIO_INT_EDGE_TO_ACTIVE);
        if (ret == 0) {
            gpio_init_callback(&button_cb_data[2], button2_isr,
                               BIT(button2.pin));
            ret = gpio_add_callback(button2.port, &button_cb_data[2]);
        }
        if (ret != 0) {
            LOG_ERR("SW2 init failed: %d", ret);
            return -1;
        }
        LOG_INF("SW2 configured (Brightness)");
    }
#endif
#if DT_NODE_HAS_STATUS(SW3_NODE, okay)
    if (gpio_is_ready_dt(&button3)) {
        ret = gpio_pin_configure_dt(&button3, GPIO_INPUT);
        if (ret == 0) ret = gpio_pin_interrupt_configure_dt(
                                &button3, GPIO_INT_EDGE_TO_ACTIVE);
        if (ret == 0) {
            gpio_init_callback(&button_cb_data[3], button3_isr,
                               BIT(button3.pin));
            ret = gpio_add_callback(button3.port, &button_cb_data[3]);
        }
        if (ret != 0) {
            LOG_ERR("SW3 init failed: %d", ret);
            return -1;
        }
        LOG_INF("SW3 configured (Status Popup)");
    }
#endif
    return 0;
}
