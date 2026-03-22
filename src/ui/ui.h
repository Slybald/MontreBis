/*
 * UI interface.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the LVGL UI components (both screens + overlays)
 */
void ui_init(void);

/* Navigation and interaction */

/**
 * @brief Cycle to the next screen: Sensors → Clock → Compass → Pedometer (Button 1)
 */
void ui_switch_screen(void);

/**
 * @brief Cycle through theme colors: Cyan -> Green -> Orange -> Magenta (Button 2)
 */
void ui_cycle_theme_color(void);

/**
 * @brief Toggle brightness overlay (dim mode) (Button 3)
 */
void ui_toggle_brightness(void);

/**
 * @brief Toggle status popup showing BLE/system info (Button 4)
 */
void ui_toggle_status_popup(void);

/**
 * @brief Show a black sleep overlay above the current screen
 */
void ui_show_sleep_overlay(void);

/**
 * @brief Hide the sleep overlay and restore the current screen
 */
void ui_hide_sleep_overlay(void);

/* Data updates */

/**
 * @brief Update temperature and humidity display
 * @param temp Temperature (Celsius)
 * @param hum Humidity (%)
 */
void ui_update_temp_humidity(float temp, float hum);

/**
 * @brief Update pressure display
 * @param press Pressure (kPa)
 */
void ui_update_pressure(float press);

/**
 * @brief Update accelerometer display
 * @param x X-axis acceleration (m/s^2)
 * @param y Y-axis acceleration (m/s^2)
 * @param z Z-axis acceleration (m/s^2)
 */
void ui_update_accel(float x, float y, float z);

/**
 * @brief Update gyroscope display
 * @param x X-axis angular rate (dps)
 * @param y Y-axis angular rate (dps)
 * @param z Z-axis angular rate (dps)
 */
void ui_update_gyro(float x, float y, float z);

/**
 * @brief Update magnetometer display
 * @param x X-axis magnetic field (gauss)
 * @param y Y-axis magnetic field (gauss)
 * @param z Z-axis magnetic field (gauss)
 */
void ui_update_magnetometer(float x, float y, float z);

/**
 * @brief Update date and time on both screens
 * @param year Year (e.g., 2026)
 * @param month Month (1-12)
 * @param day Day (1-31)
 * @param hour Hours (0-23)
 * @param minute Minutes (0-59)
 * @param second Seconds (0-59)
 */
void ui_update_datetime(int year, int month, int day, int hour, int minute, int second);

/**
 * @brief Update the calendar widget
 * @param year Year (e.g., 2026)
 * @param month Month (1-12)
 * @param day Day (1-31)
 */
void ui_update_calendar(int year, int month, int day);

/**
 * @brief Update compass heading display
 * @param heading_deg Heading in degrees (0-360, 0=North)
 */
void ui_update_compass(float heading_deg);

/**
 * @brief Update pedometer screen with step count and goal
 * @param step_count Current step count
 * @param goal Daily step goal (0 defaults to 10000)
 */
void ui_update_steps(uint32_t step_count, uint32_t goal);

/**
 * @brief Get current theme index (0..3)
 */
uint8_t ui_get_theme_index(void);

/**
 * @brief Set theme index and apply (for restoring from storage)
 */
void ui_set_theme_index(uint8_t idx);

/**
 * @brief Update BLE connection status (for status popup)
 * @param connected true if BLE client is connected
 */
void ui_set_ble_status(bool connected);

/**
 * @brief Update uptime display in status popup
 * @param uptime_sec Uptime in seconds
 */
void ui_update_uptime(uint32_t uptime_sec);

#endif /* UI_H */
