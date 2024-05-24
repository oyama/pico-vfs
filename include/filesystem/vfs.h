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

/** Open or create a file for reading or writing
 *
 * The file name specified by path is opend for reading and/or writing, as specified by the argument oflags.; the file descriptor is returned to the calling process.
 * The oflags  argument may indicate that the file is to be created if it does not exists (by specifying the O_CREAT flag).
 *
 * The flags specified for the oflags argument must include exactly one of the following file access modes:
 *  O_RDONLY     open for reading only
 *  O_WRONLY     open for writing only
 *  O_RDWR       open for reading and writing
 *
 * In addition any combination of the following values can be or'ed in oflag:
 *  O_APPEND     append on each write
 *  O_CREAT      create file if it does not exist
 *  O_TRUNC      truncate size to 0
 *
 * @param path Path of the file to be opened. Absolute paths must be specified.
 * @param oflags Specify the access mode of the file.
 * @return File descriptor
 * @retval >=0 Open succeeded
 * @retval <0 Open failed, Negative integer value indicates an error code.
 */
int fs_open(const char *path, int oflags);

/** delete a file descriptor
 *
 * @param fildes File descriptor
 * @retval 0 Close succeeded
 * @retval <0 Close failed, Negative integer value indicates an error code.
 */
int fs_close(int fildes);

/** Write output
 *
 * Write nbytes of buf content to the file descriptor of fields.
 *
 * @param fildes File descriptor
 * @param buf Write data buffer
 * @param nbyte Write data length
 * @return Length of data writed
 * @retval >=0 Write succeeded
 * @retval <0 Write failed, Negative integer value indicates an error code.
 */
ssize_t fs_write(int fildes, const void *buf, size_t nbyte);

/** Read input
 *
 * Read nbytes of data from the file descriptor of fields into buf.
 *
 * @param fildes File descriptor
 * @param buf Read data buffer
 * @param nbyte Read data length. buf must reserve a larger area than this.
 * @return Length of data readed
 * @retval >0 Read succeeded
 * @retval 0 Reading end-of-file
 * @retval <0 Read failed, Negative integer value indicates an error code.
 */
ssize_t fs_read(int fildes, void *buf, size_t nbyte);

/** Sets the file position for the file descriptor.
 *
 * @param fildes File descriptor
 * @param offset Offset bytes to the position specified by whence
 * @param whence Offset starting position. set to SEEK_SET, SEEK_CUR, or SEEK_END, the offset is relative to the start of the file.
 * @retval 0 Seek succeeded
 * @retval <0 Seek failed, Negative integer value indicates an error code.
 */
off_t fs_seek(int fildes, off_t offset, int whence);

/** Current file position of the file descriptor.
 *
 * @param fildes File descriptor
 * @return Current offset.
 * @retval <0 Tell failed, Negative integer value indicates an error code.
 */
off_t fs_tell(int fildes);

/** Truncate or extend a file to a specified length
 *
 * @param fildes File descriptor
 * @param length Truncated or extended to length bytes in size.
 * @retval 0 Truncate succeeded
 * @retval <0 Truncate failed, Negative integer value indicates an error code.
 */
int fs_truncate(int fildes, off_t length);

/** Remove directory entry
 *
 * @param path File path of the file to be deleted
 * @retval 0 Remove succeeded
 * @retval <0 Remove failed, Negative integer value indicates an error code.
 */
int fs_unlink(const char *path);

/** Change the name of a file
 *
 *  Renames the specified file name old to new.
 *
 * @param old Original file path
 * @param new New file path
 * @retval 0 Rename succeeded
 * @retval <0 Rename failed, Negative integer value indicates an error code.
 */
int fs_rename(const char *old, const char *new);

/** Make a directory file
 *
 *  The directory path is created with the access permissions specified by mode.
 *
 * @param path Directory path
 * @param mode Access permission. In practice, mode is simply ignored.
 * @retval 0 Rename succeeded
 * @retval <0 Rename failed, Negative integer value indicates an error code.
 */
int fs_mkdir(const char *path, mode_t mode);

/** Get file status
 *
 * Obtains file information for a specified path.
 *
 * @param path File path
 * @param st Pointer to stat structure
 * @retval 0 Stat Succeeded
 * @retval <0 Stat failed, Negative integer value indicates an error code.
 */
int fs_stat(const char *path, struct stat *st);

/** Open directory for reading
 *
 * @param path Directory name
 * @return Pointers directory descriptor
 * @retval NULL Open failed.
 */
fs_dir_t *fs_opendir(const char *path);

/** Close directory file descriptor
 *
 * @param dir Pointers directory descriptor
 * @retval 0 Close Succeeded
 * @retval <0 Close failed, Negative integer value indicates an error code.
 */
int fs_closedir(fs_dir_t *dir);

/** Returns a pointer to the directory entry
 *
 * @param dir Pointers directory descriptor
 * @return Pointers to directory entries
 * @retval NULL reaching the end of the directory or on error
 */
struct dirent *fs_readdir(fs_dir_t *dir);

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
