/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \defgroup filesystem_littlefs filesystem_littlefs
 *  \ingroup filesystem
 *  \brief littlefs file system
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "lfs.h"
#include "filesystem/filesystem.h"

#define LFS_DBG   0

/*! \brief Create littlefs file system object
 * \ingroup filesystem_littlefs
 *
 * \param block_cycles Number of erase cycles before littlefs evicts metadata logs and moves the metadata to another block.
 * \param lookahead_size Threshold for metadata compaction during lfs_fs_gc in bytes.
 * \return File system object. Returns NULL in case of failure.
 * \retval NULL failed to create file system object.
 */
filesystem_t *filesystem_littlefs_create(uint32_t block_cycles, lfs_size_t lookahead_size);

/*! \brief Release littlefs file system object
 * \ingroup filesystem_littlefs
 *
 * \param fs littlefs file system object
 */
void filesystem_littlefs_free(filesystem_t *fs);

#ifdef __cplusplus
}
#endif
