/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/sync.h>
#include <pico/mutex.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"


typedef struct {
    uint32_t start;
    size_t length;
    mutex_t _mutex;
} blockdevice_flash_config_t;

static const char DEVICE_NAME[] = "flash";

static size_t flash_target_offset(blockdevice_t *device) {
    blockdevice_flash_config_t *config = device->config;
    return config->start;
}

static int init(blockdevice_t *device) {
    (void)device;
    return BD_ERROR_OK;
}

static int deinit(blockdevice_t *device) {
    (void)device;
    return 0;
}
static int sync(blockdevice_t *device) {
    (void)device;
    return 0;
}

static int read(blockdevice_t *device, const void *buffer, size_t addr, size_t size) {
    blockdevice_flash_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    const uint8_t *flash_contents = (const uint8_t *)(XIP_BASE + flash_target_offset(device) + addr);
    memcpy((uint8_t *)buffer, flash_contents, size);
    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int erase(blockdevice_t *device, size_t addr, size_t size) {
    blockdevice_flash_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_target_offset(device) + addr, size);
    restore_interrupts(ints);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int program(blockdevice_t *device, const void *buffer, size_t addr, size_t size) {
    blockdevice_flash_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(flash_target_offset(device) + addr, buffer, size);
    restore_interrupts(ints);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int trim(blockdevice_t *device, size_t addr, size_t size) {
    (void)device;
    (void)addr;
    (void)size;
    return BD_ERROR_OK;
}

static size_t size(blockdevice_t *device) {
    blockdevice_flash_config_t *config = device->config;
    return config->length;
}

blockdevice_t *blockdevice_flash_create(uint32_t start, size_t length) {
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
