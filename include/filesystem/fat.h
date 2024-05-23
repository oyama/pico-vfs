/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "filesystem/filesystem.h"

#define FFS_DBG  0

filesystem_t *filesystem_fat_create();
void filesystem_fat_free(filesystem_t *fs);

#ifdef __cplusplus
}
#endif
