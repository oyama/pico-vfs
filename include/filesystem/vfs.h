/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <fcntl.h>
#include <sys/types.h>
#include "filesystem/filesystem.h"
#include "blockdevice/blockdevice.h"


int fs_format(filesystem_t *fs, blockdevice_t *device);
int fs_mount(const char *path, filesystem_t *fs, blockdevice_t *device);
int fs_unmount(const char *path);

int fs_unlink(const char *path);
int fs_rename(const char *old, const char *new);
int fs_mkdir(const char *path, mode_t mode);
int fs_stat(const char *path, struct stat *st);

int fs_open(const char *path, int oflags);
int fs_close(int fildes);
ssize_t fs_write(int fildes, const void *buf, size_t nbyte);
ssize_t fs_read(int fildes, void *buf, size_t nbyte);
off_t fs_seek(int fildes, off_t offset, int whence);
off_t fs_tell(int fildes);
int fs_truncate(int fildes, off_t length);

fs_dir_t *fs_opendir(const char *path);
int fs_closedir(fs_dir_t *dir);
struct dirent *fs_readdir(fs_dir_t *dir);

char *fs_strerror(int error);
