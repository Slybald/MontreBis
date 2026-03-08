/* RTC Time Management — SPDX-License-Identifier: Apache-2.0 */
#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <stdint.h>
#include <stdbool.h>

#define MIN_VALID_TIMESTAMP 1704067200
#define MAX_VALID_TIMESTAMP 4102444800U

enum time_source {
    TIME_SOURCE_NONE = 0,
    TIME_SOURCE_RTC,
    TIME_SOURCE_BLE,
};

enum rtc_health {
    RTC_HEALTH_OK = 0,
    RTC_HEALTH_INVALID,
    RTC_HEALTH_IO_ERROR,
};

struct time_status_snapshot {
    bool synced;
    bool rtc_present;
    enum time_source source;
    enum rtc_health rtc_health;
    uint32_t last_sync_timestamp;
    int last_rtc_error;
};

void rtc_time_init(void);
void time_sync_from_ble(uint32_t timestamp);
bool time_get_unix(uint32_t *timestamp_out);
void time_get_status(struct time_status_snapshot *out);

#endif
