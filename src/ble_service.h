/*
 * BLE Service Header
 * Custom GATT service for sensor data notifications and time sync
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Callback type for time updates
 * @param timestamp Unix timestamp received from client
 */
typedef void (*ble_time_update_cb_t)(uint32_t timestamp);

/**
 * @brief Initialize BLE stack and start advertising
 * @param time_cb Callback function to handle time write events (optional, can be NULL)
 * @return 0 on success, negative error code on failure
 */
int ble_init(ble_time_update_cb_t time_cb);

/**
 * @brief Update temperature and humidity via BLE notification
 * @param temp Temperature (Celsius * 100, integer)
 * @param hum Humidity (% * 100, integer)
 */
void ble_update_env_data(int16_t temp, uint16_t hum);

/**
 * @brief Update pressure via BLE notification
 * @param press Pressure (Pa, integer)
 */
void ble_update_pressure(uint32_t press);

/**
 * @brief Update motion data (accel/gyro) via BLE notification
 * @param ax Acceleration X (m/s^2 * 100)
 * @param ay Acceleration Y (m/s^2 * 100)
 * @param az Acceleration Z (m/s^2 * 100)
 * @param gx Gyro X (dps * 100)
 * @param gy Gyro Y (dps * 100)
 * @param gz Gyro Z (dps * 100)
 */
void ble_update_motion_data(int16_t ax, int16_t ay, int16_t az,
                            int16_t gx, int16_t gy, int16_t gz);

/**
 * @brief Update magnetometer data via BLE notification
 * @param mx Magnetometer X (milligauss)
 * @param my Magnetometer Y (milligauss)
 * @param mz Magnetometer Z (milligauss)
 */
void ble_update_magnetometer(int16_t mx, int16_t my, int16_t mz);

/**
 * @brief Update compass heading via BLE notification
 * @param heading_centideg Heading in centidegrees (0..35999)
 */
void ble_update_compass(int16_t heading_centideg);

/**
 * @brief Update step count via BLE notification
 * @param step_count Total steps
 */
void ble_update_steps(uint32_t step_count);

/**
 * @brief Check if a BLE client is connected
 * @return true if connected, false otherwise
 */
bool ble_is_connected(void);

#endif /* BLE_SERVICE_H */
