/*
 * BLE Service Implementation
 * Custom GATT service for sensor data notifications and time sync
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_service.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_service, CONFIG_LOG_DEFAULT_LEVEL);

/* Custom Service UUID: 12345678-1234-5678-1234-56789abcdef0 */
#define BT_UUID_SENSOR_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

/* Characteristic UUIDs */
#define BT_UUID_WATCH_ENV_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_WATCH_PRESS_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
#define BT_UUID_WATCH_MOTION_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
#define BT_UUID_WATCH_TIME_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)
#define BT_UUID_WATCH_MAG_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5)
#define BT_UUID_WATCH_STEPS_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef6)
#define BT_UUID_WATCH_COMPASS_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef7)

static struct bt_uuid_128 sensor_service_uuid = BT_UUID_INIT_128(BT_UUID_SENSOR_SERVICE_VAL);
static struct bt_uuid_128 env_data_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_ENV_VAL);
static struct bt_uuid_128 pressure_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_PRESS_VAL);
static struct bt_uuid_128 motion_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_MOTION_VAL);
static struct bt_uuid_128 time_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_TIME_VAL);
static struct bt_uuid_128 mag_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_MAG_VAL);
static struct bt_uuid_128 steps_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_STEPS_VAL);
static struct bt_uuid_128 compass_uuid = BT_UUID_INIT_128(BT_UUID_WATCH_COMPASS_VAL);

/* Connection state */
static struct bt_conn *current_conn;
static bool notify_env_enabled;
static bool notify_press_enabled;
static bool notify_motion_enabled;
static bool notify_mag_enabled;
static bool notify_steps_enabled;
static bool notify_compass_enabled;
static ble_time_update_cb_t time_update_callback = NULL;

/* Sensor data buffers */
static uint8_t env_data[4];      /* temp(2) + hum(2) */
static uint8_t press_data[4];    /* pressure(4) */
static uint8_t motion_data[12];  /* ax,ay,az,gx,gy,gz (2 bytes each) */
static uint8_t mag_data[6];      /* mx, my, mz (2 bytes each) */
static uint8_t steps_data[4];    /* step_count (4 bytes) */
static uint8_t compass_data[2];  /* heading centideg (2 bytes) */

/* CCC changed callbacks */
static void env_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_env_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Env notifications %s", notify_env_enabled ? "enabled" : "disabled");
}

static void press_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_press_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Pressure notifications %s", notify_press_enabled ? "enabled" : "disabled");
}

static void motion_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_motion_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Motion notifications %s", notify_motion_enabled ? "enabled" : "disabled");
}

static void mag_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_mag_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Mag notifications %s", notify_mag_enabled ? "enabled" : "disabled");
}

static void steps_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_steps_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Steps notifications %s", notify_steps_enabled ? "enabled" : "disabled");
}

static void compass_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_compass_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Compass notifications %s", notify_compass_enabled ? "enabled" : "disabled");
}

/* Read callbacks */
static ssize_t read_env(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, env_data, sizeof(env_data));
}

static ssize_t read_press(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, press_data, sizeof(press_data));
}

static ssize_t read_motion(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, motion_data, sizeof(motion_data));
}

static ssize_t read_mag(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, mag_data, sizeof(mag_data));
}

static ssize_t read_steps(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, steps_data, sizeof(steps_data));
}

static ssize_t read_compass(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, compass_data, sizeof(compass_data));
}

/* Time Write Callback */
static ssize_t write_time(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (len != 4) {
        LOG_WRN("Invalid time length: %d", len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    /* Decode 32-bit timestamp (Little Endian) */
    const uint8_t *b = buf;
    uint32_t timestamp = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);

    LOG_INF("Time received via BLE: %u", timestamp);

    if (time_update_callback) {
        time_update_callback(timestamp);
    }

    return len;
}

/* GATT Service Definition */
BT_GATT_SERVICE_DEFINE(sensor_svc,
    BT_GATT_PRIMARY_SERVICE(&sensor_service_uuid),

    /* Environmental Data (Temp + Humidity) — Read + Notify */
    BT_GATT_CHARACTERISTIC(&env_data_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_env, NULL, NULL),
    BT_GATT_CCC(env_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Pressure — Read + Notify */
    BT_GATT_CHARACTERISTIC(&pressure_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_press, NULL, NULL),
    BT_GATT_CCC(press_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Motion Data (Accel + Gyro) — Read + Notify */
    BT_GATT_CHARACTERISTIC(&motion_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_motion, NULL, NULL),
    BT_GATT_CCC(motion_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Magnetometer — Read + Notify */
    BT_GATT_CHARACTERISTIC(&mag_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_mag, NULL, NULL),
    BT_GATT_CCC(mag_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Pedometer (Step Count) — Read + Notify */
    BT_GATT_CHARACTERISTIC(&steps_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_steps, NULL, NULL),
    BT_GATT_CCC(steps_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Compass Heading — Read + Notify */
    BT_GATT_CHARACTERISTIC(&compass_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_compass, NULL, NULL),
    BT_GATT_CCC(compass_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Time (Write Only) */
    BT_GATT_CHARACTERISTIC(&time_uuid.uuid,
                           BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE,
                           NULL, write_time, NULL),
);

/* GATT layout: [0]=svc,
 * [1]=env_char, [2]=env_val, [3]=env_ccc,
 * [4]=press_char, [5]=press_val, [6]=press_ccc,
 * [7]=motion_char, [8]=motion_val, [9]=motion_ccc,
 * [10]=mag_char, [11]=mag_val, [12]=mag_ccc,
 * [13]=steps_char, [14]=steps_val, [15]=steps_ccc,
 * [16]=compass_char, [17]=compass_val, [18]=compass_ccc,
 * [19]=time_char, [20]=time_val
 */
#define ATTR_ENV_VAL     2
#define ATTR_PRESS_VAL   5
#define ATTR_MOTION_VAL  8
#define ATTR_MAG_VAL     11
#define ATTR_STEPS_VAL   14
#define ATTR_COMPASS_VAL 17

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SENSOR_SERVICE_VAL),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("BLE Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BLE Disconnected (reason %u)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    notify_env_enabled = false;
    notify_press_enabled = false;
    notify_motion_enabled = false;
    notify_mag_enabled = false;
    notify_steps_enabled = false;
    notify_compass_enabled = false;

    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Re-advertising failed (err %d)", err);
    } else {
        LOG_INF("Re-advertising started");
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_init(ble_time_update_cb_t time_cb)
{
    int err;

    LOG_INF("Initializing BLE...");

    time_update_callback = time_cb;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth initialized");

    /* Start connectable advertising using the standard macro */
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Advertising started as '%s'", CONFIG_BT_DEVICE_NAME);
    return 0;
}

void ble_update_env_data(int16_t temp, uint16_t hum)
{
    env_data[0] = temp & 0xFF;
    env_data[1] = (temp >> 8) & 0xFF;
    env_data[2] = hum & 0xFF;
    env_data[3] = (hum >> 8) & 0xFF;

    if (!current_conn || !notify_env_enabled) {
        return;
    }

    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_ENV_VAL], env_data, sizeof(env_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Env notify failed: %d", err);
    }
}

void ble_update_pressure(uint32_t press)
{
    press_data[0] = press & 0xFF;
    press_data[1] = (press >> 8) & 0xFF;
    press_data[2] = (press >> 16) & 0xFF;
    press_data[3] = (press >> 24) & 0xFF;

    if (!current_conn || !notify_press_enabled) {
        return;
    }

    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_PRESS_VAL], press_data, sizeof(press_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Pressure notify failed: %d", err);
    }
}

void ble_update_motion_data(int16_t ax, int16_t ay, int16_t az,
                            int16_t gx, int16_t gy, int16_t gz)
{
    motion_data[0] = ax & 0xFF;
    motion_data[1] = (ax >> 8) & 0xFF;
    motion_data[2] = ay & 0xFF;
    motion_data[3] = (ay >> 8) & 0xFF;
    motion_data[4] = az & 0xFF;
    motion_data[5] = (az >> 8) & 0xFF;
    motion_data[6] = gx & 0xFF;
    motion_data[7] = (gx >> 8) & 0xFF;
    motion_data[8] = gy & 0xFF;
    motion_data[9] = (gy >> 8) & 0xFF;
    motion_data[10] = gz & 0xFF;
    motion_data[11] = (gz >> 8) & 0xFF;

    if (!current_conn || !notify_motion_enabled) {
        return;
    }

    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_MOTION_VAL], motion_data, sizeof(motion_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Motion notify failed: %d", err);
    }
}

void ble_update_magnetometer(int16_t mx, int16_t my, int16_t mz)
{
    mag_data[0] = mx & 0xFF;
    mag_data[1] = (mx >> 8) & 0xFF;
    mag_data[2] = my & 0xFF;
    mag_data[3] = (my >> 8) & 0xFF;
    mag_data[4] = mz & 0xFF;
    mag_data[5] = (mz >> 8) & 0xFF;

    if (!current_conn || !notify_mag_enabled) return;
    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_MAG_VAL], mag_data, sizeof(mag_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Mag notify failed: %d", err);
    }
}

void ble_update_compass(int16_t heading_centideg)
{
    compass_data[0] = heading_centideg & 0xFF;
    compass_data[1] = (heading_centideg >> 8) & 0xFF;

    if (!current_conn || !notify_compass_enabled) return;
    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_COMPASS_VAL], compass_data, sizeof(compass_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Compass notify failed: %d", err);
    }
}

void ble_update_steps(uint32_t step_count)
{
    steps_data[0] = step_count & 0xFF;
    steps_data[1] = (step_count >> 8) & 0xFF;
    steps_data[2] = (step_count >> 16) & 0xFF;
    steps_data[3] = (step_count >> 24) & 0xFF;

    if (!current_conn || !notify_steps_enabled) return;
    int err = bt_gatt_notify(current_conn, &sensor_svc.attrs[ATTR_STEPS_VAL], steps_data, sizeof(steps_data));
    if (err && err != -ENOTCONN) {
        LOG_WRN("Steps notify failed: %d", err);
    }
}

bool ble_is_connected(void)
{
    return (current_conn != NULL);
}
