/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/dirent.h>
#include "filesystem/vfs.h"


typedef struct {
    const char *dir;
    void *filesystem;
} mountpoint_t;

typedef struct {
    fs_file_t file;
    filesystem_t *filesystem;
} file_descriptor_t;

typedef struct {
    fs_dir_t dir;
    filesystem_t *filesystem;
} dir_descriptor_t;


#define FS_MOUNTPOINT_NUM   10
static mountpoint_t mountpoints[FS_MOUNTPOINT_NUM] = {0};
static file_descriptor_t file_descriptors[10] = {0};
static dir_descriptor_t dir_descriptors[10] = {0};


static const char *remove_prefix(const char *str, const char *prefix) {
    size_t len_prefix = strlen(prefix);
    size_t len_str = strlen(str);

    if (len_str < len_prefix || strncmp(str, prefix, len_prefix) != 0) {
        return str;
    }
    return str + len_prefix;
}


static mountpoint_t *find_mountpoint(const char *path) {
    mountpoint_t *longest_match = NULL;
    size_t longest_length = 0;

    for (size_t i = 0; i < FS_MOUNTPOINT_NUM; i++) {
        size_t prefix_length = strlen(mountpoints[i].dir);
        if (prefix_length > longest_length && strncmp(path, mountpoints[i].dir, prefix_length) == 0) {
            longest_match = &mountpoints[i];
            longest_length = prefix_length;
        }
    }
    return longest_match;
}


int fs_format(filesystem_t *fs, blockdevice_t *device) {
    return fs->format(fs, device);
}

int fs_mount(const char *dir, filesystem_t *fs, blockdevice_t *device) {
    int err = fs->mount(fs, device);
    if (err) {
        return err;
    }

    for (size_t i = 0; i < FS_MOUNTPOINT_NUM; i++) {
        if (mountpoints[i].filesystem == NULL) {
            mountpoints[i].filesystem = fs;
            mountpoints[i].dir = strdup(dir);
            return 0;
        }
    }
    return -1;
}

int fs_unmount(const char *path) {
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return -ENOENT;
    }
    filesystem_t *fs = mp->filesystem;
    int err = fs->unmount(fs);
    if (err) {
        return err;
    }

    mp->filesystem = NULL;
    free((char *)mp->dir);
    return 0;
}

int _unlink(const char *path) {
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return -ENOENT;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    return fs->remove(fs, entity_path);
}

int rename(const char *old, const char *new) {
    // TODO: Check if old and new are the same filesystem
    mountpoint_t *mp = find_mountpoint(old);
    if (mp == NULL) {
        return -ENOENT;
    }
    const char *old_entity_path = remove_prefix(old, mp->dir);
    const char *new_entity_path = remove_prefix(new, mp->dir);

    filesystem_t *fs = mp->filesystem;
    return fs->rename(fs, old_entity_path, new_entity_path);
}

int mkdir(const char *path, mode_t mode) {
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return -ENOENT;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    return fs->mkdir(fs, entity_path, mode);
}

int _stat(const char *path, struct stat *st) {
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return -ENOENT;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    return fs->stat(fs, entity_path, st);
}

int _fstat(int fildes, struct stat *st) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL) {
        return -EBADF;
    }

    off_t current = fs->file_tell(fs, file);
    off_t size = fs->file_seek(fs, file, 0, SEEK_END);
    off_t err = fs->file_seek(fs, file, current, SEEK_SET);
    if (current != err) {
        return err;
    }

    st->st_size = size;
    st->st_mode = S_IFREG;
    return 0;
}


int _open(const char *path, int oflags, ...) {
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return -ENOENT;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    // find file descriptor
    int fd = -1;
    for (int i = 0; i < 10; i++) {
        if (file_descriptors[i].filesystem == NULL) {
            fd = i;
            break;
        }
    }
    filesystem_t *fs = mp->filesystem;
    fs_file_t *file = &file_descriptors[fd].file;
    int err = fs->file_open(fs, file, entity_path, oflags);
    if (err < 0) {
        return err;
    }
    file_descriptors[fd].filesystem = fs;
    return fd;
}

int _close(int fildes) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL) {
        return -EBADF;
    }
    int err = fs->file_close(fs, file);
    file_descriptors[fildes].filesystem = NULL;
    return err;
}

ssize_t _write(int fildes, const void *buf, size_t nbyte) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL) {
        return -EBADF;
    }
    return fs->file_write(fs, file, buf, nbyte);
}

ssize_t _read(int fildes, void *buf, size_t nbyte) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL)
        return -EBADF;

    return fs->file_read(fs, file, buf, nbyte);
}

off_t _lseek(int fildes, off_t offset, int whence) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL)
        return -EBADF;

    return fs->file_seek(fs, file, offset, whence);
}

off_t _ftello_r(struct _reent *ptr, register FILE *fp) {
    (void)ptr;
    int fildes = fp->_file;

    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL)
        return -EBADF;

    return fs->file_tell(fs, file);
}

int ftruncate(int fildes, off_t length) {
    fs_file_t *file = &file_descriptors[fildes].file;
    filesystem_t *fs = file_descriptors[fildes].filesystem;
    if (fs == NULL)
        return -EBADF;

    return fs->file_truncate(fs, file, length);
}

DIR *opendir(const char *path) {

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        return NULL;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    // find dir descriptor
    int fd = -1;
    for (int i = 0; i < 10; i++) {
        if (dir_descriptors[i].filesystem == NULL) {
            fd = i;
            break;
        }
    }
    fs_dir_t *dir = &dir_descriptors[fd].dir;
    filesystem_t *fs = mp->filesystem;
    int err = fs->dir_open(fs, dir, entity_path);
    if (err != 0) {
        return NULL;
    }
    dir_descriptors[fd].filesystem = fs;
    dir->fd = fd;
    return dir;
}

int closedir(DIR *dir) {

    int fd = dir->fd;
    fs_dir_t *_dir = &dir_descriptors[fd].dir;
    filesystem_t *fs = dir_descriptors[fd].filesystem;
    if (fs == NULL) {
        return -EBADF;
    }
    int err = fs->dir_close(fs, _dir);
    dir_descriptors[fd].filesystem = NULL;
    return err;
}

struct dirent *readdir(DIR *dir) {

    fs_dir_t *_dir = &dir_descriptors[dir->fd].dir;
    filesystem_t *fs = dir_descriptors[dir->fd].filesystem;
    if (fs == NULL)
        return NULL;
    memset(&_dir->current, 0, sizeof(_dir->current));
    int err = fs->dir_read(fs, _dir, &_dir->current);
    if (err == 0) {
        return &_dir->current;
    } else if (err == -ENOENT) {
        memset(&_dir->current, 0, sizeof(_dir->current));
        return NULL;
    } else {
        memset(&_dir->current, 0, sizeof(_dir->current));
        return NULL;
    }
}

char *fs_strerror(int error) {
    return strerror(-error);
}
