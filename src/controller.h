/* Controller FSM — SPDX-License-Identifier: Apache-2.0 */
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <zephyr/drivers/display.h>

void controller_set_display(const struct device *dev);

#endif
