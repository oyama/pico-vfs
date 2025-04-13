/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <FreeRTOS.h>
#include <errno.h>
#include <pico/stdlib.h>
#include <portable.h>
#include <stdio.h>
#include <string.h>
#include <task.h>
#include "filesystem/vfs.h"

#define BENCHMARK_SIZE       (0.4 * 1024 * 1024)
#define BUFFER_SIZE          (1024 * 2)
#define CORE_ACCESSING_FLASH 1            // core0

static uint32_t xor_rand(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static uint32_t xor_rand_32bit(uint32_t *seed) {
    return xor_rand(seed);
}

void benchmark_task(void *p) {
    const char *path = p;
    printf("start benchmark %s on core%d\n", path, get_core_num());

    absolute_time_t start_at = get_absolute_time();
    int fd = open(path, O_WRONLY|O_CREAT);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    uint32_t counter = 0;
    xor_rand(&counter);
    uint8_t buffer[BUFFER_SIZE] = {0};
    size_t remind = BENCHMARK_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        uint32_t *b = (uint32_t *)buffer;
        for (size_t j = 0; j < (chunk / sizeof(uint32_t)); j++) {
            b[j] = xor_rand_32bit(&counter);
        }

        ssize_t write_size = write(fd, buffer, chunk);
        if (write_size == -1) {
            printf("write error: %s\n", strerror(errno));
            return;
        }
        remind = remind - write_size;
    }

    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }
    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf("Write %s: %.1f KB/s\n", path, (double)(BENCHMARK_SIZE) / duration / 1024);


    start_at = get_absolute_time();
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    counter = 0;
    xor_rand(&counter);
    remind = BENCHMARK_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        ssize_t read_size = read(fd, buffer, chunk);
        if (read_size == -1) {
            printf("read error: %s\n", strerror(errno));
            return;
        }
        uint32_t *b = (uint32_t *)buffer;
        for (size_t j = 0; j < chunk / sizeof(uint32_t); j++) {
            volatile uint32_t v = xor_rand_32bit(&counter);
            if (b[j] != v) {
                printf("data mismatch\n");
                return;
            }
        }

        remind = remind - read_size;
    }

    err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }
    duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf("Read %s: %.1f KB/s\n", path, (double)(BENCHMARK_SIZE) / duration / 1024);


    while (true) {
       ;
    }
}

int main(void) {
    stdio_init_all();
    fs_init();
    printf("FreeRTOS benchmark\n");

    xTaskCreate(benchmark_task, "SD Card", 1024*1, "/sd/benchmark", 1, NULL);

    TaskHandle_t task_handle;
    xTaskCreate(benchmark_task, "flash", 1024*1, "/flash/benchmark", 1, &(task_handle));
    vTaskCoreAffinitySet(task_handle, CORE_ACCESSING_FLASH);

    vTaskStartScheduler();

    while (true)
        ;
}
