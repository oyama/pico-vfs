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


/*! \brief Create FAT file system object
 *
 * Create FAT file system object.
 *
 * \return File system object. Returns NULL in case of failure.
 * \retval NULL failed to create file system object.
 */
filesystem_t *filesystem_fat_create();

/*! \brief Release FAT file system object
 *
 * \param fs FAT file system object
 */
void filesystem_fat_free(filesystem_t *fs);

#ifdef __cplusplus
}
#endif
