/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "blockdevice/blockdevice.h"

#define PATH_MAX   256

typedef enum {
    FILESYSTEM_TYPE_FAT,
    FILESYSTEM_TYPE_LITTLEFS,
} filesystem_type_t;

enum {
    DT_UNKNOWN = 0,
    DT_DIR     = 4,
    DT_REG     = 8,
};

struct dirent {
    uint8_t d_type;
    char d_name[255 + 1];
};

typedef struct {
    int fd;
    void *context;
} fs_file_t;

typedef struct {
    int fd;
    void *context;
    struct dirent current;
} fs_dir_t;


typedef struct filesystem {
    filesystem_type_t type;
    const char *name;
    void *context;

    int (*mount)(struct filesystem *fs, blockdevice_t *device);
    int (*unmount)(struct filesystem *fs);
    int (*format)(struct filesystem *fs, blockdevice_t *device);

    int (*remove)(struct filesystem *fs, const char *path);
    int (*rename)(struct filesystem *fs, const char *oldpath, const char *newpath);
    int (*mkdir)(struct filesystem *fs, const char *path, mode_t mode);
    int (*stat)(struct filesystem *fs, const char *path, struct stat *st);

    int (*file_open)(struct filesystem *fs, fs_file_t *file, const char *path, int flags);
    int (*file_close)(struct filesystem *fs, fs_file_t *file);
    ssize_t (*file_write)(struct filesystem *fs, fs_file_t *file, const void *buffer, size_t size);
    ssize_t (*file_read)(struct filesystem *fs, fs_file_t *file, void *buffer, size_t size);
    int (*file_sync)(struct filesystem *fs, fs_file_t *file);
    off_t (*file_seek)(struct filesystem *fs, fs_file_t *file, off_t offset, int whence);
    off_t (*file_tell)(struct filesystem *fs, fs_file_t *file);
    off_t (*file_size)(struct filesystem *fs, fs_file_t *file);
    int (*file_truncate)(struct filesystem *fs, fs_file_t *file, off_t length);

    int (*dir_open)(struct filesystem *fs, fs_dir_t *dir, const char *path);
    int (*dir_close)(struct filesystem *fs, fs_dir_t *dir);
    int (*dir_read)(struct filesystem *fs, fs_dir_t *dir, struct dirent *ent);
} filesystem_t;

#ifdef __cplusplus
}
#endif
