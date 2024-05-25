/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <sys/types.h>
#include "filesystem/filesystem.h"
#include "blockdevice/blockdevice.h"


#if !defined(DEFAULT_FS_SIZE)
#define DEFAULT_FS_SIZE    (512 * 1024)
#endif

/** Enable predefined file systems
 *
 * This default function defines the block device and file system, formats it if necessary and then mounts it on `/` to make it available.
 * The `pico_enable_filesystem` function in CMakeLists.txt provides a default or user-defined fs_init function.
 *
 * @retval true Init succeeded.
 * @retval false Init failed.
 */
bool fs_init(void);

/** Format block device with specify file system
 *
 * Block devices can be formatted and made available as a file system.
 *
 * @param fs File system object. Format the block device according to the specified file system.
 * @param device Block device used in the file system.
 * @retval 0 Format succeeded.
 * @retval <0 Format failed. Negative integer value indicates an error code.
 */
int fs_format(filesystem_t *fs, blockdevice_t *device);

/** Mount a file system
 *
 * Mounts a file system with block devices at the specified path.
 *
 * @param path Directory path of the mount point. Specify a string beginning with a slash.
 * @param fs File system object.
 * @param device Block device used in the file system. Block devices must be formatted with a file system.
 * @retval 0 Mount succeeded.
 * @retval <0 Mount failed. Negative integer value indicates an error code.
 */
int fs_mount(const char *path, filesystem_t *fs, blockdevice_t *device);

/** Dismount a file system
 *
 * Dismount a file system.
 *
 * @param path Directory path of the mount point. Must be the same as the path specified for the mount.
 * @retval 0 Dismount succeeded.
 * @retval <0 Dismount failed. Negative integer value indicates an error code.
 */
int fs_unmount(const char *path);

/** File system error message
 *
 * Convert the error code reported in the negative integer into a string.
 *
 * @param error Negative error code returned by the file system.
 * @return Pointer to the corresponding message string.
 */
char *fs_strerror(int error);

#ifdef __cplusplus
}
#endif
