/*
 * Multi-core logger that saves 1kHz sampling data to an SD card
 *
 * This code is not specifically tuned. The data is substituted with random numbers,
 * and the actual application will probably be reading the ADC or communicating with
 * the sensor chip. Also, the method of outputting CSV files is slow. The method of
 * outputting binary data is faster.
 *
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <pico/util/queue.h>
#include <stdio.h>
#include <string.h>
#include "filesystem/vfs.h"

#if !defined(SAMPLING_RATE_HZ)
#define SAMPLING_RATE_HZ  1000
#endif

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float mag_x;
    float mag_y;
    float mag_z;
    uint64_t timestamp;
} sensor_data_t;

queue_t sensor_queue;
static char write_buffer[1024 * 8];

static float normal_random(float mean, float stddev) {
    static bool has_spare = false;
    static float spare;

    if (has_spare) {
        has_spare = false;
        return mean + stddev * spare;
    }

    has_spare = true;
    float u, v, s;
    do {
        u = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        v = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    return mean + stddev * u * s;
}

static bool sampling_task(repeating_timer_t *t) {
    (void)t;
    sensor_data_t entry = {0};

    entry.timestamp = (uint64_t)get_absolute_time();
    entry.accel_x = normal_random(0, 0.01);
    entry.accel_y = normal_random(0, 0.01);
    entry.accel_z = normal_random(-1.0, 0.01);
    entry.gyro_x = normal_random(0, 0.001);
    entry.gyro_y = normal_random(0, 0.001);
    entry.gyro_z = normal_random(0, 0.001);
    entry.mag_x = normal_random(-0.25, 0.0001);
    entry.mag_y = normal_random(0.1, 0.01);
    entry.mag_z = normal_random(0.4, 0.01);

    queue_try_add(&sensor_queue, &entry);
    return true;
}

static void produce_sensor_data_task(void) {
    repeating_timer_t timer;
    add_repeating_timer_us((int64_t)((1.0 / SAMPLING_RATE_HZ) * -1000000),
                           &sampling_task, NULL, &timer);
    while (true)
        __wfi();
}

int main(void) {
    stdio_init_all();
    if (!fs_init()) {
        printf("SD card device not found\n");
        return -1;
    }

    queue_init(&sensor_queue, sizeof(sensor_data_t), 1024);
    FILE *fp = fopen("/sd/sensor_data.csv", "w");
    if (fp == NULL) {
        printf("fopen failed: %s\n", strerror(errno));
    }
    setvbuf(fp, write_buffer, _IOFBF, sizeof(write_buffer));
    fprintf(fp, "Time,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,MagX,MagY,Magz\n");

    multicore_reset_core1();
    sleep_ms(100);
    multicore_launch_core1(produce_sensor_data_task);

    sensor_data_t entry = {0};
    absolute_time_t last_checkpoint = get_absolute_time();
    uint32_t n = 0;
    while (1) {
        queue_remove_blocking(&sensor_queue, &entry);
        fprintf(fp, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                (double)entry.timestamp / 1000 / 1000,
                entry.accel_x, entry.accel_y, entry.accel_z,
                entry.gyro_x, entry.gyro_y, entry.gyro_x,
                entry.mag_x, entry.mag_y, entry.mag_z);
        n += 1;
        absolute_time_t now = get_absolute_time();
        int64_t duration = absolute_time_diff_us(last_checkpoint, now);
        if (duration >= 1000000) {
            printf("Store %lu samples/sec, Remaining queue %u\n",
                   n, queue_get_level(&sensor_queue));
            n = 0;
            last_checkpoint = now;
        }
    }

    fclose(fp);
    queue_free(&sensor_queue);

    while (1)
        tight_loop_contents();
}
