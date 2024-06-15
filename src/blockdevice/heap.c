/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
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

#include <ctype.h>
static void print_hex(const char *label, const void *buffer, size_t length) {
    const uint8_t *buf = buffer;
    printf("%s:\n", label);
    size_t offset = 0;
    for (size_t i = 0; i < length; ++i) {
        if (i % 16 == 0)
            printf("0x%04u%s", offset, (i % 512) == 0 ? ">" : " ");
        if (isalnum(buf[i])) {
            printf("'%c' ", buf[i]);
        } else {
            printf("0x%02x", buf[i]);
        }
        if (i % 16 == 15) {
            printf("\n");
            offset += 16;
        } else {
            printf(", ");
        }
    }
}

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

static int read(blockdevice_t *device, const void *buffer, size_t addr, size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    memcpy((uint8_t *)buffer, config->heap + addr, length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int erase(blockdevice_t *device, size_t addr, size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    assert(config->heap != NULL);
    memset(config->heap + addr, PICO_VFS_BLOCKDEVICE_HEAP_ERASE_VALUE, length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int program(blockdevice_t *device, const void *buffer, size_t addr, size_t length) {
    blockdevice_heap_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    memcpy(config->heap + addr, buffer, length);

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int trim(blockdevice_t *device, size_t addr, size_t length) {
    (void)device;
    (void)addr;
    (void)length;
    return BD_ERROR_OK;
}

static size_t size(blockdevice_t *device) {
    blockdevice_heap_config_t *config = device->config;
    return config->size;
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
