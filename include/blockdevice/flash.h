/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <stdint.h>
#include "blockdevice/blockdevice.h"


typedef struct {
    uint32_t start;
    size_t length;
} blockdevice_flash_config_t;

blockdevice_t *blockdevice_flash_create(uint32_t start, size_t length);
void blockdevice_flash_free(blockdevice_t *device);
