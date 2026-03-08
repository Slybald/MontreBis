/* Sensor Thread — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "app_events.h"
#include "sensors.h"

LOG_MODULE_REGISTER(sensors, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *hts221_dev;
static const struct device *lps22hh_dev;
static const struct device *lis2mdl_dev;
static const struct device *lsm6dso_dev;

static void event_post_log(enum event_type type)
{
    if (event_post(type) == -EBUSY) {
        LOG_WRN("Event bus full, dropped type=%d", type);
    }
}

static void sensors_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("[SENSORS] Thread started (P7)");

    while (1) {
        k_sem_take(&sem_sensor_start, K_FOREVER);

        struct sensor_snapshot snap = {0};

        /* HTS221: Temperature + Humidity */
        if (device_is_ready(hts221_dev)) {
            if (sensor_sample_fetch(hts221_dev) == 0) {
                struct sensor_value temp, hum;
                sensor_channel_get(hts221_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
                sensor_channel_get(hts221_dev, SENSOR_CHAN_HUMIDITY, &hum);
                snap.temperature = sensor_value_to_float(&temp);
                snap.humidity    = sensor_value_to_float(&hum);
                snap.valid_flags |= SNAP_VALID_ENV;
            } else {
                LOG_WRN("HTS221 fetch failed");
            }
        }

        /* LPS22HH: Pressure */
        if (device_is_ready(lps22hh_dev)) {
            if (sensor_sample_fetch(lps22hh_dev) == 0) {
                struct sensor_value press;
                sensor_channel_get(lps22hh_dev, SENSOR_CHAN_PRESS, &press);
                snap.pressure = sensor_value_to_float(&press);
                snap.valid_flags |= SNAP_VALID_PRESS;
            } else {
                LOG_WRN("LPS22HH fetch failed");
            }
        }

        /* LSM6DSO: Accelerometer + Gyroscope */
        if (device_is_ready(lsm6dso_dev)) {
            if (sensor_sample_fetch(lsm6dso_dev) == 0) {
                struct sensor_value ax, ay, az, gx, gy, gz;
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_ACCEL_X, &ax);
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_ACCEL_Y, &ay);
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_ACCEL_Z, &az);
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_GYRO_X,  &gx);
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_GYRO_Y,  &gy);
                sensor_channel_get(lsm6dso_dev, SENSOR_CHAN_GYRO_Z,  &gz);
                snap.accel[0] = sensor_value_to_float(&ax);
                snap.accel[1] = sensor_value_to_float(&ay);
                snap.accel[2] = sensor_value_to_float(&az);
                snap.gyro[0]  = sensor_value_to_float(&gx);
                snap.gyro[1]  = sensor_value_to_float(&gy);
                snap.gyro[2]  = sensor_value_to_float(&gz);
                snap.valid_flags |= SNAP_VALID_MOTION;
            } else {
                LOG_WRN("LSM6DSO fetch failed");
            }
        }

        /* LIS2MDL: Magnetometer */
        if (device_is_ready(lis2mdl_dev)) {
            if (sensor_sample_fetch(lis2mdl_dev) == 0) {
                struct sensor_value mx, my, mz;
                sensor_channel_get(lis2mdl_dev, SENSOR_CHAN_MAGN_X, &mx);
                sensor_channel_get(lis2mdl_dev, SENSOR_CHAN_MAGN_Y, &my);
                sensor_channel_get(lis2mdl_dev, SENSOR_CHAN_MAGN_Z, &mz);
                snap.magn[0] = sensor_value_to_float(&mx);
                snap.magn[1] = sensor_value_to_float(&my);
                snap.magn[2] = sensor_value_to_float(&mz);
                snap.valid_flags |= SNAP_VALID_MAG;
            } else {
                LOG_WRN("LIS2MDL fetch failed");
            }
        }

        k_mutex_lock(&mtx_sensors, K_FOREVER);
        shared_sensors = snap;
        k_mutex_unlock(&mtx_sensors);

        event_post_log(EVT_SENSOR_DATA_READY);
    }
}

K_THREAD_DEFINE(sensors_tid, 4096, sensors_entry,
                NULL, NULL, NULL, 7, 0, 0);

void sensors_init(void)
{
    hts221_dev  = DEVICE_DT_GET(DT_NODELABEL(hts221_x_nucleo_iks01a3));
    lps22hh_dev = DEVICE_DT_GET(DT_NODELABEL(lps22hh_x_nucleo_iks01a3));
    lis2mdl_dev = DEVICE_DT_GET(DT_NODELABEL(lis2mdl_1e_x_nucleo_iks01a3));
    lsm6dso_dev = DEVICE_DT_GET(DT_NODELABEL(lsm6dso_6b_x_nucleo_iks01a3));

    if (!device_is_ready(hts221_dev))  LOG_WRN("HTS221 not ready");
    if (!device_is_ready(lps22hh_dev)) LOG_WRN("LPS22HH not ready");
    if (!device_is_ready(lis2mdl_dev)) LOG_WRN("LIS2MDL not ready");
    if (!device_is_ready(lsm6dso_dev)) LOG_WRN("LSM6DSO not ready");

    LOG_INF("Sensor devices bound");
}
