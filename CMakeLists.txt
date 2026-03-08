# Copyright (c) 2019 STMicroelectronics
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Default shields configuration if not provided by build system
if(NOT DEFINED SHIELD)
    set(SHIELD adafruit_2_8_tft_touch_v2 x_nucleo_iks01a3)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(connected_watch)

target_sources(app PRIVATE
    src/main.c
    src/input_tsc2007.c
    src/buttons.c
    src/rtc_time.c
    src/sensors.c
    src/controller.c
    src/display_thread.c
    src/ble_service.c
    src/storage.c
    src/ui/ui.c
)
