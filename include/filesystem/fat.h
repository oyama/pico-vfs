/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "blockdevice/blockdevice.h"
#include "filesystem/filesystem.h"
#include "pico_vfs_conf.h"
#include "ff.h"

#define FFS_DBG  0


typedef struct {
    FIL file;
} fat_file_t;

typedef struct {
    FATFS fatfs;
    int id;
} filesystem_fat_context_t;

filesystem_t *filesystem_fat_create();
void filesystem_fat_free(filesystem_t *fs);
