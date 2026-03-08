/* RTC Time Management — SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>
#include <time.h>

#include "rtc_time.h"
#include "storage.h"

LOG_MODULE_REGISTER(rtc_time, CONFIG_LOG_DEFAULT_LEVEL);

#define RTC_READ_ATTEMPTS       5
#define RTC_READ_RETRY_DELAY_MS 50

static const struct device *rtc_dev;

struct time_state {
    int64_t offset_sec;
    bool synced;
    bool rtc_present;
    enum time_source source;
    enum rtc_health rtc_health;
    uint32_t last_sync_timestamp;
    int last_rtc_error;
};

static struct time_state g_time_state = {
    .offset_sec = 0,
    .synced = false,
    .rtc_present = false,
    .source = TIME_SOURCE_NONE,
    .rtc_health = RTC_HEALTH_IO_ERROR,
    .last_sync_timestamp = 0,
    .last_rtc_error = -ENODEV,
};

K_MUTEX_DEFINE(mtx_time);

static time_t rtc_time_to_unix(const struct rtc_time *rt)
{
    struct tm t = {0};
    t.tm_sec  = rt->tm_sec;
    t.tm_min  = rt->tm_min;
    t.tm_hour = rt->tm_hour;
    t.tm_mday = rt->tm_mday;
    t.tm_mon  = rt->tm_mon;
    t.tm_year = rt->tm_year;
    t.tm_isdst = -1;
    return mktime(&t);
}

static void unix_to_rtc_time(time_t timestamp, struct rtc_time *rt)
{
    struct tm t;
    gmtime_r(&timestamp, &t);
    rt->tm_sec   = t.tm_sec;
    rt->tm_min   = t.tm_min;
    rt->tm_hour  = t.tm_hour;
    rt->tm_mday  = t.tm_mday;
    rt->tm_mon   = t.tm_mon;
    rt->tm_year  = t.tm_year;
    rt->tm_wday  = t.tm_wday;
    rt->tm_yday  = t.tm_yday;
    rt->tm_isdst = t.tm_isdst;
    rt->tm_nsec  = 0;
}

void rtc_time_init(void)
{
    int err = -ENODEV;
    struct rtc_time rt;

    k_mutex_lock(&mtx_time, K_FOREVER);
    g_time_state.last_sync_timestamp = storage_load_last_sync();
    g_time_state.synced = false;
    g_time_state.source = TIME_SOURCE_NONE;
    g_time_state.rtc_present = false;
    g_time_state.rtc_health = RTC_HEALTH_IO_ERROR;
    g_time_state.last_rtc_error = -ENODEV;
    k_mutex_unlock(&mtx_time);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(rv8263), okay)
    rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rv8263));
#else
    rtc_dev = NULL;
    LOG_WRN("RTC node not found in device tree");
    return;
#endif

    if (!rtc_dev || !device_is_ready(rtc_dev)) {
        LOG_ERR("RTC device not ready — check I2C2 wiring");
        return;
    }

    k_mutex_lock(&mtx_time, K_FOREVER);
    g_time_state.rtc_present = true;
    k_mutex_unlock(&mtx_time);

    LOG_INF("RTC ready (RV-8263-C8 on I2C2)");

    for (int attempt = 0; attempt < RTC_READ_ATTEMPTS; attempt++) {
        err = rtc_get_time(rtc_dev, &rt);
        if (err == 0 || err == -ENODATA) {
            break;
        }
        if (attempt < (RTC_READ_ATTEMPTS - 1)) {
            k_msleep(RTC_READ_RETRY_DELAY_MS);
        }
    }

    if (err == 0) {
        time_t stored = rtc_time_to_unix(&rt);
        if (stored >= MIN_VALID_TIMESTAMP && stored <= MAX_VALID_TIMESTAMP) {
            int64_t up_s = k_uptime_get() / 1000;
            k_mutex_lock(&mtx_time, K_FOREVER);
            g_time_state.offset_sec = (int64_t)stored - up_s;
            g_time_state.synced = true;
            g_time_state.source = TIME_SOURCE_RTC;
            g_time_state.rtc_health = RTC_HEALTH_OK;
            g_time_state.last_rtc_error = 0;
            k_mutex_unlock(&mtx_time);
            LOG_INF("RTC time restored: %u", (uint32_t)stored);
        } else {
            k_mutex_lock(&mtx_time, K_FOREVER);
            g_time_state.synced = false;
            g_time_state.source = TIME_SOURCE_NONE;
            g_time_state.rtc_health = RTC_HEALTH_INVALID;
            g_time_state.last_rtc_error = -ENODATA;
            k_mutex_unlock(&mtx_time);
            LOG_WRN("RTC time invalid (%u), waiting for BLE sync",
                    (uint32_t)stored);
        }
    } else if (err == -ENODATA) {
        k_mutex_lock(&mtx_time, K_FOREVER);
        g_time_state.synced = false;
        g_time_state.source = TIME_SOURCE_NONE;
        g_time_state.rtc_health = RTC_HEALTH_INVALID;
        g_time_state.last_rtc_error = err;
        k_mutex_unlock(&mtx_time);
        LOG_WRN("RTC contains invalid data, waiting for BLE sync");
    } else {
        k_mutex_lock(&mtx_time, K_FOREVER);
        g_time_state.synced = false;
        g_time_state.source = TIME_SOURCE_NONE;
        g_time_state.rtc_health = RTC_HEALTH_IO_ERROR;
        g_time_state.last_rtc_error = err;
        k_mutex_unlock(&mtx_time);
        LOG_ERR("RTC read failed: %d", err);
    }
}

void time_sync_from_ble(uint32_t timestamp)
{
    if (timestamp < MIN_VALID_TIMESTAMP || timestamp > MAX_VALID_TIMESTAMP) {
        LOG_WRN("Rejected invalid BLE timestamp: %u", timestamp);
        return;
    }

    int64_t up_s = k_uptime_get() / 1000;
    int64_t offset = (int64_t)timestamp - up_s;

    k_mutex_lock(&mtx_time, K_FOREVER);
    g_time_state.offset_sec = offset;
    g_time_state.synced = true;
    g_time_state.source = TIME_SOURCE_BLE;
    g_time_state.last_sync_timestamp = timestamp;
    k_mutex_unlock(&mtx_time);

    storage_save_last_sync(timestamp);

    LOG_INF("Time synced via BLE: %u (offset=%lld)", timestamp, (long long)offset);

    if (rtc_dev && device_is_ready(rtc_dev)) {
        struct rtc_time rt;
        unix_to_rtc_time((time_t)timestamp, &rt);
        struct rtc_time verify;
        int err = rtc_set_time(rtc_dev, &rt);
        if (err == 0) {
            err = rtc_get_time(rtc_dev, &verify);
            if (err == 0) {
                k_mutex_lock(&mtx_time, K_FOREVER);
                g_time_state.rtc_present = true;
                g_time_state.rtc_health = RTC_HEALTH_OK;
                g_time_state.last_rtc_error = 0;
                k_mutex_unlock(&mtx_time);
                LOG_INF("RTC hardware updated");
            } else {
                k_mutex_lock(&mtx_time, K_FOREVER);
                g_time_state.rtc_present = true;
                g_time_state.rtc_health = RTC_HEALTH_IO_ERROR;
                g_time_state.last_rtc_error = err;
                k_mutex_unlock(&mtx_time);
                LOG_ERR("RTC readback failed: %d", err);
            }
        } else {
            k_mutex_lock(&mtx_time, K_FOREVER);
            g_time_state.rtc_present = true;
            g_time_state.rtc_health = RTC_HEALTH_IO_ERROR;
            g_time_state.last_rtc_error = err;
            k_mutex_unlock(&mtx_time);
            LOG_ERR("RTC write failed: %d", err);
        }
    }
}

bool time_get_unix(uint32_t *timestamp_out)
{
    bool synced;
    int64_t offset_sec;
    int64_t now_s;

    k_mutex_lock(&mtx_time, K_FOREVER);
    synced = g_time_state.synced;
    offset_sec = g_time_state.offset_sec;
    k_mutex_unlock(&mtx_time);

    if (!synced || timestamp_out == NULL) {
        return false;
    }

    now_s = k_uptime_get() / 1000;
    *timestamp_out = (uint32_t)(offset_sec + now_s);

    return synced;
}

void time_get_status(struct time_status_snapshot *out)
{
    if (out == NULL) {
        return;
    }

    k_mutex_lock(&mtx_time, K_FOREVER);
    out->synced = g_time_state.synced;
    out->rtc_present = g_time_state.rtc_present;
    out->source = g_time_state.source;
    out->rtc_health = g_time_state.rtc_health;
    out->last_sync_timestamp = g_time_state.last_sync_timestamp;
    out->last_rtc_error = g_time_state.last_rtc_error;
    k_mutex_unlock(&mtx_time);
}
