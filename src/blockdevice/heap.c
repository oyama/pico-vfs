/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <pico/mutex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/heap.h"

#if !defined(PICO_VFS_BLOCKDEVICE_HEAP_BLOCK_SIZE)
#define PICO_VFS_BLOCKDEVICE_HEAP_BLOCK_SIZE        512
#endif

#if !defined(PICO_VFS_BLOCKDEVICE_HEAP_ERASE_VALUE)
#define PICO_VFS_BLOCKDEVICE_HEAP_ERASE_VALUE      0xFF
#endif

typedef struct {
    size_t size;
    uint8_t *heap;
    mutex_t _mutex;
} blockdevice_heap_config_t;

static const char DEVICE_NAME[] = "heap";


static int init(blockdevice_t *device) {
    (void)device;
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);
    if (device->is_initialized) {
        mutex_exit(&config->_mutex);
        return BD_ERROR_OK;
    }

    config->heap = malloc(config->size);  // To reproduce device contamination, malloc instead of calloc
    if (config->heap == NULL) {
        mutex_exit(&config->_mutex);
        return -errno;  // Low layer errors in pico-vfs use negative error codes
    }

    device->is_initialized = true;

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int deinit(blockdevice_t *device) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    if (!device->is_initialized) {
        mutex_exit(&config->_mutex);
        return BD_ERROR_OK;
    }

    if (config->heap)
        free(config->heap);
    config->heap = NULL;
    device->is_initialized = false;

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int sync(blockdevice_t *device) {
    (void)device;
    return BD_ERROR_OK;
}

static int read(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    memcpy((uint8_t *)buffer, config->heap + (size_t)addr, (size_t)length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int erase(blockdevice_t *device, bd_size_t addr, bd_size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    assert(config->heap != NULL);
    memset(config->heap + (size_t)addr, PICO_VFS_BLOCKDEVICE_HEAP_ERASE_VALUE, (size_t)length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int program(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    memcpy(config->heap + (size_t)addr, buffer, (size_t)length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int trim(blockdevice_t *device, bd_size_t addr, bd_size_t length) {
    (void)device;
    (void)addr;
    (void)length;
    return BD_ERROR_OK;
}

static bd_size_t size(blockdevice_t *device) {
    blockdevice_heap_config_t *config = device->config;
    return (bd_size_t)config->size;
}

blockdevice_t *blockdevice_heap_create(size_t length) {
    blockdevice_t *device = calloc(1, sizeof(blockdevice_t));
    if (device == NULL) {
        return NULL;
    }
    blockdevice_heap_config_t *config = calloc(1, sizeof(blockdevice_heap_config_t));
    if (config == NULL) {
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
    device->read_size = PICO_VFS_BLOCKDEVICE_HEAP_BLOCK_SIZE;
    device->erase_size = PICO_VFS_BLOCKDEVICE_HEAP_BLOCK_SIZE;
    device->program_size = PICO_VFS_BLOCKDEVICE_HEAP_BLOCK_SIZE;
    device->name = DEVICE_NAME;
    device->is_initialized = false;

    config->size = length;
    config->heap = NULL;
    mutex_init(&config->_mutex);
    device->config = config;
    device->init(device);
    return device;
}

void blockdevice_heap_free(blockdevice_t *device) {
    device->deinit(device);
    free(device->config);
    free(device);
}
