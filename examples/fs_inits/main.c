/*
 * Benchmark tests
 *
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <fcntl.h>
#include <hardware/clocks.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>
#include "filesystem/vfs.h"


#define COLOR_GREEN(format)   ("\e[32m" format "\e[0m")
#define BENCHMARK_SIZE        (0.6 * 1024 * 1024)
#define BENCHMARK_SIZE_SMALL  (256 * 1024)
#define BENCHMARK_SIZE_TINY   (100 * 1024)


static uint32_t xor_rand(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static uint32_t xor_rand_32bit(uint32_t *seed) {
    return xor_rand(seed);
}

static size_t get_benchmark_size(const char *path) {
    blockdevice_t *device = NULL;
    filesystem_t *fs = NULL;

    int err = fs_info(path, &fs, &device);
    if (err == -1) {
        printf("fs_info error: %s", strerror(errno));
        return false;
    }
    if (strcmp(device->name, "flash") == 0 || strcmp(device->name, "sd") == 0)
        return BENCHMARK_SIZE;
    else if (strcmp(device->name, "heap") == 0)
        return BENCHMARK_SIZE_TINY;
    else
        return BENCHMARK_SIZE_SMALL;
}

static void benchmark_write(void) {
    printf("Write ");
    size_t benchmark_size = get_benchmark_size("/");
    uint64_t start_at = get_absolute_time();

    int fd = open("/benchmark", O_WRONLY|O_CREAT);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    uint32_t counter = 0;
    xor_rand(&counter);
    uint8_t buffer[1024*16] = {0};
    size_t remind = benchmark_size;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        uint32_t *b = (uint32_t *)buffer;
        for (size_t j = 0; j < (chunk / sizeof(uint32_t)); j++) {
            b[j] = xor_rand_32bit(&counter);
        }
        ssize_t write_size = write(fd, buffer, chunk);
        if (write_size == -1) {
            printf("write: error: %s\n", strerror(errno));
            return;
        }
        printf(".");
        remind = remind - write_size;
    }

    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(COLOR_GREEN(" %.1f KB/s\n"), (double)(benchmark_size) / duration / 1024);
}

static void benchmark_read(void) {
    printf("Read  ");
    size_t benchmark_size = get_benchmark_size("/");
    uint64_t start_at = get_absolute_time();

    int fd = open("/benchmark", O_RDONLY);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        return;
    }

    uint32_t counter = 0;
    xor_rand(&counter);
    uint8_t buffer[1024*16] = {0};
    size_t remind = benchmark_size;
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
        printf(".");
        remind = remind - read_size;
    }

    int err = close(fd);
    if (err == -1) {
        printf("close error: %s\n", strerror(errno));
        return;
    }

    double duration = (double)absolute_time_diff_us(start_at, get_absolute_time()) / 1000 / 1000;
    printf(COLOR_GREEN(" %.1f KB/s\n"), (double)(benchmark_size) / duration / 1024);
}

int main(void) {
    stdio_init_all();
    fs_init();

    benchmark_write();
    benchmark_read();
}
