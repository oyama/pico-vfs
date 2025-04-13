/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \file fat.h
 *  \defgroup filesystem_fat filesystem_fat
 *  \ingroup filesystem
 *  \brief FAT file system
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "filesystem/filesystem.h"

#define FFS_DBG  0


/*! \brief Create FAT file system object
 * \ingroup filesystem_fat
 *
 * \return File system object. Returns NULL in case of failure.
 * \retval NULL failed to create file system object.
 */
filesystem_t *filesystem_fat_create();

/*! \brief Release FAT file system object
 * \ingroup filesystem_fat
 *
 * \param fs FAT file system object
 */
void filesystem_fat_free(filesystem_t *fs);

#ifdef __cplusplus
}
#endif
