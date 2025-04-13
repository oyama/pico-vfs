/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \file blockdevice.h
 * \defgroup blockdevice blockdevice
 * \brief Block device abstraction layer for storage media
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint64_t bd_size_t;

enum bd_error {
    BD_ERROR_OK                 = 0,     /*!< no error */
    BD_ERROR_DEVICE_ERROR       = -4001, /*!< device specific error */
};

/*! \brief block device abstract object
 *  \ingroup blockdevice
 *
 *  All block device objects implement blockdevice_t
 */
typedef struct blockdevice {
    int (*init)(struct blockdevice *device);
    int (*deinit)(struct blockdevice *device);
    int (*sync)(struct blockdevice *device);
    int (*read)(struct blockdevice *device, const void *buffer, bd_size_t addr, bd_size_t size);
    int (*program)(struct blockdevice *device, const void *buffer, bd_size_t addr, bd_size_t size);
    int (*erase)(struct blockdevice *device, bd_size_t addr, bd_size_t size);
    int (*trim)(struct blockdevice *device, bd_size_t addr, bd_size_t size);
    bd_size_t (*size)(struct blockdevice *device);
    size_t read_size;
    size_t erase_size;
    size_t program_size;
    const char *name;
    void *config;
    bool is_initialized;
} blockdevice_t;

#ifdef __cplusplus
}
#endif
