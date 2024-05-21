/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <fcntl.h>
#include "lfs.h"
#include "blockdevice/blockdevice.h"
#include "filesystem/filesystem.h"

#define LFS_DBG   0


typedef struct {
   lfs_file_t file;
} littlefs_file_t;

typedef struct {
    lfs_t littlefs;
    struct lfs_config config;
    int id;
} filesystem_littlefs_context_t;

filesystem_t *filesystem_littlefs_create(uint32_t block_cycles, lfs_size_t lookahead_size);
void filesystem_littlefs_free(filesystem_t *fs);
