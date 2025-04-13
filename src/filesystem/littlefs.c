/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <fcntl.h>
#include <pico/mutex.h>
#include "lfs.h"
#include "blockdevice/blockdevice.h"
#include "filesystem/littlefs.h"


typedef struct {
   lfs_file_t file;
} littlefs_file_t;

typedef struct {
    lfs_t littlefs;
    struct lfs_config config;
    int id;
    mutex_t _mutex;
} filesystem_littlefs_context_t;

static const char FILESYSTEM_NAME[] = "littlefs";

static int _error_remap(int err) {
    switch (err) {
    case LFS_ERR_OK:
        return 0;
    case LFS_ERR_IO:
        return -EIO;
    case LFS_ERR_NOENT:
        return -ENOENT;
    case LFS_ERR_EXIST:
        return -EEXIST;
    case LFS_ERR_NOTDIR:
        return -ENOTDIR;
    case LFS_ERR_ISDIR:
        return -EISDIR;
    case LFS_ERR_INVAL:
        return -EINVAL;
    case LFS_ERR_NOSPC:
        return -ENOSPC;
    case LFS_ERR_NOMEM:
        return -ENOMEM;
    case LFS_ERR_CORRUPT:
        return -EILSEQ;
    default:
        return err;
    }
}

static int _flags_remap(int flags) {
    return ((((flags & 3) == O_RDONLY) ? LFS_O_RDONLY : 0) |
            (((flags & 3) == O_WRONLY) ? LFS_O_WRONLY : 0) |
            (((flags & 3) == O_RDWR)   ? LFS_O_RDWR   : 0) |
            ((flags & O_CREAT)  ? LFS_O_CREAT  : 0) |
            ((flags & O_EXCL)   ? LFS_O_EXCL   : 0) |
            ((flags & O_TRUNC)  ? LFS_O_TRUNC  : 0) |
            ((flags & O_APPEND) ? LFS_O_APPEND : 0));
}

static int _whence_remap(int whence) {
    switch (whence) {
    case SEEK_SET:
        return LFS_SEEK_SET;
    case SEEK_CUR:
        return LFS_SEEK_CUR;
    case SEEK_END:
        return LFS_SEEK_END;
    default:
        return whence;
    }
}

static int _mode_remap(int type) {
    int mode = S_IRWXU | S_IRWXG | S_IRWXO;
    switch (type) {
    case LFS_TYPE_DIR:
        return mode | S_IFDIR;
    case LFS_TYPE_REG:
        return mode | S_IFREG;
    default:
        return 0;
    }
}

static int _type_remap(int type) {
    switch (type) {
    case LFS_TYPE_DIR:
        return DT_DIR;
    case LFS_TYPE_REG:
        return DT_REG;
    default:
        return DT_UNKNOWN;
    }
}

static int littlefs_read(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, void *buffer, lfs_size_t size)
{
    blockdevice_t *device = c->context;
    return device->read(device, buffer, (size_t)block * c->block_size + off, size);
}

static int littlefs_erase(const struct lfs_config *c, lfs_block_t block) {
    blockdevice_t *device = c->context;
    return device->erase(device, block * c->block_size, c->block_size);
}

static int littlefs_program(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, const void *buffer, lfs_size_t size)
{
    blockdevice_t *device = c->context;
    return device->program(device, buffer, block * c->block_size + off, size);
}

static int littlefs_sync(const struct lfs_config *c) {
    blockdevice_t *device = c->context;
    return device->sync(device);
}

static void _init_config(struct lfs_config *config, blockdevice_t *device) {
    int32_t block_cycles = config->block_cycles;
    lfs_size_t lookahead_size = config->lookahead_size;
    memset(config, 0, sizeof(struct lfs_config));
    config->block_cycles = block_cycles;
    config->lookahead_size = lookahead_size;

    config->read = littlefs_read;
    config->prog = littlefs_program;
    config->erase = littlefs_erase;
    config->sync = littlefs_sync;
    config->read_size = device->read_size;
    config->prog_size = device->program_size;
    config->block_size = device->erase_size;
    config->block_count = device->size(device) / config->block_size;
    config->cache_size = device->erase_size;
    config->context = device;
}

static int format(filesystem_t *fs, blockdevice_t *device) {
    filesystem_littlefs_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex);

    int err = device->init(device);
    if (err) {
        mutex_exit(&context->_mutex);
        return err;
    }

    // erase super block
    err = device->erase(device, 0, device->program_size);
    if (err) {
        mutex_exit(&context->_mutex);
        return err;
    }

    _init_config(&context->config, device);
    err = lfs_format(&context->littlefs, &context->config);
    if (err) {
        mutex_exit(&context->_mutex);
        return _error_remap(err);
    }

    mutex_exit(&context->_mutex);
    return 0;
}

static int mount(filesystem_t *fs, blockdevice_t *device, bool pending) {
    (void)pending;
    filesystem_littlefs_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex);

    int err = device->init(device);
    if (err) {
        mutex_exit(&context->_mutex);
        return err;
    }

    _init_config(&context->config, device);
    err = lfs_mount(&context->littlefs, &context->config);
    if (err) {
        mutex_exit(&context->_mutex);
        return _error_remap(err);
    }
    mutex_exit(&context->_mutex);
    return 0;
}

static int unmount(filesystem_t *fs) {
    filesystem_littlefs_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex);

    int res = 0;
    int err = lfs_unmount(&context->littlefs);
    if (err && !res) {
        res = _error_remap(err);
    }

    mutex_exit(&context->_mutex);
    return res;
}

static int file_remove(filesystem_t *fs, const char *filename) {
    filesystem_littlefs_context_t *context = fs->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_remove(&context->littlefs, filename);
    mutex_exit(&context->_mutex);

    return _error_remap(err);
}

static int file_rename(filesystem_t *fs, const char *oldpath, const char *newpath) {
    filesystem_littlefs_context_t *context = fs->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_rename(&context->littlefs, oldpath, newpath);
    mutex_exit(&context->_mutex);

    return _error_remap(err);
}

static int file_mkdir(filesystem_t *fs, const char *path, mode_t mode) {
    (void)mode;
    filesystem_littlefs_context_t *context = fs->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_mkdir(&context->littlefs, path);
    mutex_exit(&context->_mutex);

    return _error_remap(err);
}

static int file_rmdir(filesystem_t *fs, const char *path) {
    filesystem_littlefs_context_t *context = fs->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_remove(&context->littlefs, path);
    mutex_exit(&context->_mutex);

    return _error_remap(err);
}

static int file_stat(filesystem_t *fs, const char *path, struct stat *st) {
    filesystem_littlefs_context_t *context = fs->context;
    struct lfs_info info = {0};

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_stat(&context->littlefs, path, &info);
    mutex_exit(&context->_mutex);

    st->st_size = info.size;
    st->st_mode = _mode_remap(info.type);
    return _error_remap(err);
}

static int file_open(filesystem_t *fs, fs_file_t *file, const char *path, int flags) {
    filesystem_littlefs_context_t *context = fs->context;

    littlefs_file_t *f = file->context = calloc(1, sizeof(littlefs_file_t));
    if (f == NULL) {
        fprintf(stderr, "file_open: Out of memory\n");
        return -ENOMEM;
    }

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_file_open(&context->littlefs, &f->file, path, _flags_remap(flags));
    mutex_exit(&context->_mutex);

    if (err) {
        free(f);
    }
    return _error_remap(err);
}

static int file_close(filesystem_t *fs, fs_file_t *file) {
    filesystem_littlefs_context_t *context = fs->context;
    littlefs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_file_close(&context->littlefs, &f->file);
    mutex_exit(&context->_mutex);

    free(f);
    file->context = NULL;
    return _error_remap(err);
}

static ssize_t file_read(filesystem_t *fs, fs_file_t *file, void *buffer, size_t len) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    lfs_ssize_t res = lfs_file_read(&context->littlefs, f, buffer, len);
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static ssize_t file_write(filesystem_t *fs, fs_file_t *file, const void *buffer, size_t len) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    lfs_ssize_t res = lfs_file_write(&context->littlefs, f, buffer, len);
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static int file_sync(filesystem_t *fs, fs_file_t *file) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_file_sync(&context->littlefs, f);
    mutex_exit(&context->_mutex);

    return _error_remap(err);
}

static off_t file_seek(filesystem_t *fs, fs_file_t *file, off_t offset, int whence) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = lfs_file_seek(&context->littlefs, f, offset, _whence_remap(whence));
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static off_t file_tell(filesystem_t *fs, fs_file_t *file) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = lfs_file_tell(&context->littlefs, f);
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static off_t file_size(filesystem_t *fs, fs_file_t *file) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = lfs_file_size(&context->littlefs, f);
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static int file_truncate(filesystem_t *fs, fs_file_t *file, off_t length) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_file_t *f = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = lfs_file_truncate(&context->littlefs, f, length);
    mutex_exit(&context->_mutex);

    return _error_remap(res);
}

static int dir_open(filesystem_t *fs, fs_dir_t *dir, const char *path) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_dir_t *d = calloc(1, sizeof(lfs_dir_t));
    if (d == NULL) {
        fprintf(stderr, "dir_open: Out of memory\n");
        return -ENOMEM;
    }

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_dir_open(&context->littlefs, d, path);
    mutex_exit(&context->_mutex);

    if (!err) {
        dir->context = d;
        dir->fd = -1;
    } else {
        free(d);
    }
    return _error_remap(err);
}

static int dir_close(filesystem_t *fs, fs_dir_t *dir) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_dir_t *d = dir->context;

    mutex_enter_blocking(&context->_mutex);
    int err = lfs_dir_close(&context->littlefs, d);
    mutex_exit(&context->_mutex);

    free(d);
    return _error_remap(err);
}

static int dir_read(filesystem_t *fs, fs_dir_t *dir, struct dirent *ent) {
    filesystem_littlefs_context_t *context = fs->context;
    lfs_dir_t *d = dir->context;
    struct lfs_info info;

    mutex_enter_blocking(&context->_mutex);
    int res = lfs_dir_read(&context->littlefs, d, &info);
    mutex_exit(&context->_mutex);

    if (res == 1) {
        ent->d_type = _type_remap(info.type);
        strcpy(ent->d_name, info.name);
        return _error_remap(LFS_ERR_OK);
    } else if (res == 0) {
        return _error_remap(LFS_ERR_NOENT);
    }
    return _error_remap(res);
}

filesystem_t *filesystem_littlefs_create(uint32_t block_cycles,
                                         lfs_size_t lookahead_size)
{
    filesystem_t *fs = calloc(1, sizeof(filesystem_t));
    if (fs == NULL) {
        fprintf(stderr, "filesystem_littlefs_create: Out of memory\n");
        return NULL;
    }

    fs->type = FILESYSTEM_TYPE_LITTLEFS;
    fs->name = FILESYSTEM_NAME;
    fs->mount = mount;
    fs->unmount = unmount;
    fs->format = format;
    fs->remove = file_remove;
    fs->rename = file_rename;
    fs->mkdir = file_mkdir;
    fs->rmdir = file_rmdir;
    fs->stat = file_stat;
    fs->file_open = file_open;
    fs->file_close = file_close;
    fs->file_write = file_write;
    fs->file_read = file_read;
    fs->file_sync = file_sync;
    fs->file_seek = file_seek;
    fs->file_tell = file_tell;
    fs->file_size = file_size;
    fs->file_truncate = file_truncate;
    fs->dir_open = dir_open;
    fs->dir_close = dir_close;
    fs->dir_read = dir_read;

    filesystem_littlefs_context_t *context = calloc(1, sizeof(filesystem_littlefs_context_t));
    if (context == NULL) {
        fprintf(stderr, "filesystem_littlefs_create: Out of memory\n");
        free(fs);
        return NULL;
    }
    context->id = -1;
    context->config.block_cycles = block_cycles;
    context->config.lookahead_size = lookahead_size;
    mutex_init(&context->_mutex);
    fs->context = context;
    return fs;
}

void filesystem_littlefs_free(filesystem_t *fs) {
    free(fs->context);
    fs->context = NULL;
    free(fs);
    fs = NULL;
}
