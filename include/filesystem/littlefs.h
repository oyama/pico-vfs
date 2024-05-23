/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lfs.h"
#include "filesystem/filesystem.h"

#define LFS_DBG   0

filesystem_t *filesystem_littlefs_create(uint32_t block_cycles, lfs_size_t lookahead_size);
void filesystem_littlefs_free(filesystem_t *fs);

#ifdef __cplusplus
}
#endif
