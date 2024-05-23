/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "blockdevice/blockdevice.h"

blockdevice_t *blockdevice_flash_create(uint32_t start, size_t length);
void blockdevice_flash_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
