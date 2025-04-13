/*
 * Copyright 2024, Hiroyuki OYAMA
 *
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


#if !defined(PICO_FS_DEFAULT_SIZE)
#define PICO_FS_DEFAULT_SIZE         (1408 * 1024)   // Can share storage with MicroPython for RP2
#endif

/*! \brief Enable predefined file systems
 * \ingroup filesystem
 *
 * This default function defines the block device and file system, formats it if necessary and then mounts it on `/` to make it available.
 * The `pico_enable_filesystem` function in CMakeLists.txt provides a default or user-defined fs_init function.
 *
 * \retval true Init succeeded.
 * \retval false Init failed.
 */
bool fs_init(void);

/*! \brief Format block device with specify file system
 * \ingroup filesystem
 *
 * Block devices can be formatted and made available as a file system.
 *
 * \param fs File system object. Format the block device according to the specified file system.
 * \param device Block device used in the file system.
 * \retval 0 Format succeeded.
 * \retval -1 Format failed. Error codes are indicated by errno.
 */
int fs_format(filesystem_t *fs, blockdevice_t *device);

/*! \brief Mount a file system
 * \ingroup filesystem
 *
 * Mounts a file system with block devices at the specified path.
 *
 * \param path Directory path of the mount point. Specify a string beginning with a slash.
 * \param fs File system object.
 * \param device Block device used in the file system. Block devices must be formatted with a file system.
 * \retval 0 Mount succeeded.
 * \retval -1 Mount failed. Error codes are indicated by errno.
 */
int fs_mount(const char *path, filesystem_t *fs, blockdevice_t *device);

/*! \brief Dismount a file system
 * \ingroup filesystem
 *
 * Dismount a file system.
 *
 * \param path Directory path of the mount point. Must be the same as the path specified for the mount.
 * \retval 0 Dismount succeeded.
 * \retval -1 Dismount failed. Error codes are indicated by errno.
 */
int fs_unmount(const char *path);

/*! \brief Reformat the mounted file system
 * \ingroup filesystem
 *
 * Reformat a file system mounted at the specified path.
 *
 * \param path Directory path of the mount point. Must be the same as the path specified for the mount.
 * \retval 0 Reformat suceeded.
 * \retval -1 Reformat failed. Error codes are indicated by errno.
 */
int fs_reformat(const char *path);

/*! \brief Lookup filesystem and blockdevice objects from a mount point
 * \ingroup filesystem
 *
 * \param path Directory path of the mount point. Must be the same as the path specified for the mount.
 * \param fs Pointer references to filesystem objects
 * \param device Pinter references to blockdevice objects
 * \retval 0 Lookup succeeded
 * \retval -1 Lookup failed. Error codes are indicated by errno.
 */
int fs_info(const char *path, filesystem_t **fs, blockdevice_t **device);

/*! \brief File system error message
 * \ingroup filesystem
 *
 * Convert the error code reported in the negative integer into a string.
 *
 * \param error Negative error code returned by the file system.
 * \return Pointer to the corresponding message string.
 */
char *fs_strerror(int error);

#ifdef __cplusplus
}
#endif
