/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pico/mutex.h>
#include "blockdevice/loopback.h"

typedef struct {
    const char *path;
    size_t capacity;
    size_t block_size;
    int fildes;
    mutex_t _mutex;
} blockdevice_loopback_config_t;

static const char DEVICE_NAME[] = "loopback";

static int init(blockdevice_t *device) {
    (void)device;
    blockdevice_loopback_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);
    if (device->is_initialized) {
        mutex_exit(&config->_mutex);
        return BD_ERROR_OK;
    }

    config->fildes = open(config->path, O_RDWR|O_CREAT, 0644);
    if (config->fildes == -1 && errno == EEXIST) {
        config->fildes = open(config->path, O_RDWR);
    }
    if (config->fildes == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }

    device->is_initialized = true;
    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int deinit(blockdevice_t *device) {
    blockdevice_loopback_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    if (!device->is_initialized) {
        mutex_exit(&config->_mutex);
        return BD_ERROR_OK;
    }
    int err = close(config->fildes);
    if (err == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }
    device->is_initialized = false;
    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int __sync(blockdevice_t *device) {
    (void)device;
    return BD_ERROR_OK;
}

static int __read(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t length) {
    blockdevice_loopback_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    off_t off = lseek(config->fildes, addr, SEEK_SET);
    if (off == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }

    ssize_t read_size = read(config->fildes, (void *)buffer, (size_t)length);
    if (read_size == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }
    if ((size_t)read_size < (size_t)length) {
        size_t remind = length - read_size;
        memset((void *)buffer + read_size, 0, remind);
    }

    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static int erase(blockdevice_t *device, bd_size_t addr, bd_size_t length) {
    (void)device;
    (void)addr;
    (void)length;
    return BD_ERROR_OK;
}

static int program(blockdevice_t *device, const void *buffer, bd_size_t addr, bd_size_t length) {
    blockdevice_loopback_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    off_t off = lseek(config->fildes, addr, SEEK_SET);
    if (off == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }
    ssize_t write_size = write(config->fildes, buffer, (size_t)length);
    if (write_size == -1) {
        mutex_exit(&config->_mutex);
        return -errno;
    }

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
    blockdevice_loopback_config_t *config = device->config;
    return (bd_size_t)config->capacity;
}

blockdevice_t *blockdevice_loopback_create(const char *path, size_t capacity, size_t block_size) {
    blockdevice_t *device = calloc(1, sizeof(blockdevice_t));
    if (device == NULL) {
        return NULL;
    }
    blockdevice_loopback_config_t *config = calloc(1, sizeof(blockdevice_loopback_config_t));
    if (config == NULL) {
        free(device);
        return NULL;
    }

    device->init = init;
    device->deinit = deinit;
    device->read = __read;
    device->erase = erase;
    device->program = program;
    device->trim = trim;
    device->sync = __sync;
    device->size = size;
    device->read_size = block_size;
    device->erase_size = block_size;
    device->program_size = block_size;
    device->name = DEVICE_NAME;
    device->is_initialized = false;

    config->path = path;
    config->capacity = capacity;
    config->block_size = block_size;
    config->fildes = -1;

    mutex_init(&config->_mutex);
    device->config = config;
    if (device->init(device) != BD_ERROR_OK) {
        free(config);
        free(device);
        return NULL;
    }
    return device;
}

void blockdevice_loopback_free(blockdevice_t *device) {
    device->deinit(device);
    free(device->config);
    free(device);
}
