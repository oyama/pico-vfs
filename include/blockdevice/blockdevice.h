/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

enum bd_error {
    BD_ERROR_OK                 = 0,     /*!< no error */
    BD_ERROR_DEVICE_ERROR       = -4001, /*!< device specific error */
};

typedef struct blockdevice {
    int (*init)(struct blockdevice *device);
    int (*deinit)(struct blockdevice *device);
    int (*sync)(struct blockdevice *device);
    int (*read)(struct blockdevice *device, const void *buffer, size_t addr, size_t size);
    int (*program)(struct blockdevice *device, const void *buffer, size_t addr, size_t size);
    int (*erase)(struct blockdevice *device, size_t addr, size_t size);
    int (*trim)(struct blockdevice *device, size_t addr, size_t size);
    size_t (*size)(struct blockdevice *device);
    size_t read_size;
    size_t erase_size;
    size_t program_size;
    const char *name;
    void *config;
} blockdevice_t;
