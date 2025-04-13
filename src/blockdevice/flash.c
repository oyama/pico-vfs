/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/flash.h>
#include <pico/mutex.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"

#define FLASH_SAFE_EXECUTE_TIMEOUT   10 * 1000

#define FLASH_BLOCK_DEVICE_ERROR_TIMEOUT                -4001  /*!< operation timeout */
#define FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED          -4002  /*!< safe execution is not possible */
#define FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES -4003 /*!< method fails due to dynamic resource exhaustion */

typedef struct {
    uint32_t start;
    size_t length;
    mutex_t _mutex;
} blockdevice_flash_config_t;

typedef struct {
    bool is_erase;
    size_t addr;
    size_t size;
    void *buffer;
} _safe_flash_update_param_t;

static const char DEVICE_NAME[] = "flash";

static int _error_remap(int err) {
    switch (err) {
    case PICO_OK:
        return BD_ERROR_OK;
    case PICO_ERROR_TIMEOUT:
        return FLASH_BLOCK_DEVICE_ERROR_TIMEOUT;
    case PICO_ERROR_NOT_PERMITTED:
        return FLASH_BLOCK_DEVICE_ERROR_NOT_PERMITTED;
    case PICO_ERROR_INSUFFICIENT_RESOURCES:
        return FLASH_BLOCK_DEVICE_ERROR_INSUFFICIENT_RESOURCES;
    default:
        return err;
    }
}

static size_t flash_target_offset(blockdevice_t *device) {
    blockdevice_flash_config_t *config = device->config;
    return config->start;
}

static int init(blockdevice_t *device) {
    device->is_initialized = true;
    return BD_ERROR_OK;
}

static int deinit(blockdevice_t *device) {
    device->is_initialized = false;

    return 0;
}

static int sync(blockdevice_t *device) {
    (void)device;
    return 0;
}

static int read(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t size) {
    blockdevice_flash_config_t *config = device->config;

    mutex_enter_blocking(&config->_mutex);
    const uint8_t *flash_contents = (const uint8_t *)(XIP_BASE + flash_target_offset(device) + (size_t)addr);
    memcpy((uint8_t *)buffer, flash_contents, (size_t)size);
    mutex_exit(&config->_mutex);

    return BD_ERROR_OK;
}

static void _safe_flash_update(void *param) {
    const _safe_flash_update_param_t *args = param;
    if (args->is_erase) {
        flash_range_erase(args->addr, args->size);
    } else {
        flash_range_program(args->addr, (const uint8_t *)args->buffer, args->size);
    }
}

static int erase(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    _safe_flash_update_param_t param = {
        .is_erase = true,
        .addr = flash_target_offset(device) + addr,
        .size = (size_t)size,
    };
    int err = flash_safe_execute(_safe_flash_update, &param, FLASH_SAFE_EXECUTE_TIMEOUT);
    return _error_remap(err);
}

static int program(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t size) {
    _safe_flash_update_param_t param = {
        .is_erase = false,
        .addr = flash_target_offset(device) + addr,
        .buffer = (void *)buffer,
        .size = (size_t)size,
    };
    int err = flash_safe_execute(_safe_flash_update, &param, FLASH_SAFE_EXECUTE_TIMEOUT);
    return _error_remap(err);
}

static int trim(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    (void)device;
    (void)addr;
    (void)size;
    return BD_ERROR_OK;
}

static bd_size_t size(blockdevice_t *device) {
    blockdevice_flash_config_t *config = device->config;
    return (bd_size_t)config->length;
}

blockdevice_t *blockdevice_flash_create(uint32_t start, size_t length) {
    assert(start % FLASH_SECTOR_SIZE == 0);
    assert(length % FLASH_SECTOR_SIZE == 0);

    blockdevice_t *device = calloc(1, sizeof(blockdevice_t));
    if (device == NULL) {
        fprintf(stderr, "blockdevice_flash_create: Out of memory\n");
        return NULL;
    }
    blockdevice_flash_config_t *config = calloc(1, sizeof(blockdevice_flash_config_t));
    if (config == NULL) {
        fprintf(stderr, "blockdevice_flash_create: Out of memory\n");
        free(device);
        return NULL;
    }

    device->init = init;
    device->deinit = deinit;
    device->read = read;
    device->erase = erase;
    device->program = program;
    device->trim = trim;
    device->sync = sync;
    device->size = size;
    device->read_size = 1;
    device->erase_size = FLASH_SECTOR_SIZE;  // 4096 byte
    device->program_size = FLASH_PAGE_SIZE;  // 256 byte
    device->name = DEVICE_NAME;
    device->is_initialized = false;

    config->start = start;
    config->length = length > 0 ? length : (PICO_FLASH_SIZE_BYTES - start);
    mutex_init(&config->_mutex);
    device->config = config;
    device->init(device);
    return device;
}

void blockdevice_flash_free(blockdevice_t *device) {
    free(device->config);
    free(device);
}
