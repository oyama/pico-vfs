/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hardware/rtc.h>
#include <lwip/apps/sntp.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <stdint.h>

#define NTP_SERVER "pool.ntp.org"

bool ntp_sync() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_init();

    for (int i = 0; i < 20; i++) {
        if (sntp_getreachability(0)) {
            return true;
        }
        sleep_ms(1000);
    }
    return false;
}

void set_system_time(uint32_t sec) {
    time_t epoch = sec;
    struct tm *time = gmtime(&epoch);
    datetime_t dt = {
            .year = (int16_t) (1900 + time->tm_year),
            .month = (int8_t) (time->tm_mon + 1),
            .day = (int8_t) time->tm_mday,
            .hour = (int8_t) time->tm_hour,
            .min = (int8_t) time->tm_min,
            .sec = (int8_t) time->tm_sec,
            .dotw = (int8_t) time->tm_wday,
    };

    rtc_set_datetime(&dt);
    struct timeval tv = {.tv_sec = mktime(time)};
    settimeofday(&tv, NULL);
}

const char *get_timestamp() {
    static char buffer[30];
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
    return buffer;
}
