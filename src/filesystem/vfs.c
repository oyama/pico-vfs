/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/unistd.h>
#include <pico/mutex.h>
#include "filesystem/vfs.h"


typedef struct {
    const char *dir;
    void *filesystem;
    void *device;
} mountpoint_t;

typedef struct {
    fs_file_t *file;
    filesystem_t *filesystem;
    char path[PATH_MAX + 1];
} file_descriptor_t;

typedef struct {
    fs_dir_t *dir;
    filesystem_t *filesystem;
} dir_descriptor_t;

#if !defined(PICO_VFS_MAX_MOUNTPOINT)
#define PICO_VFS_MAX_MOUNTPOINT        10
#endif
#define FS_MAX_MOUNTPOINT              PICO_VFS_MAX_MOUNTPOINT
#define STDIO_FILNO_MAX                STDERR_FILENO
#define FILENO_VALUE(fd)               (fd + STDIO_FILNO_MAX + 1)  // Conversion to file descriptors for publication
#define FILENO_INDEX(fd)               (fd - STDIO_FILNO_MAX - 1)  // Conversion to file descriptors for internal use

static mountpoint_t mountpoints[FS_MAX_MOUNTPOINT] = {0};  // Mount points and file system map
static size_t max_file_descriptor = 0;                     // File descriptor current maximum value
static file_descriptor_t *file_descriptor = NULL;          // File descriptor and file system map
static size_t max_dir_descriptor = 0;                      // Dir descriptor current maximum value
static dir_descriptor_t *dir_descriptor = NULL;            // Dir descriptor and file system map
static recursive_mutex_t __attribute__((unused)) _mutex;   // Recursive mutexes are used because recursive calls occur, e.g. on loopback devices

static int _error_remap(int err) {
    if (err >= 0) {
        errno = 0;
        return err;
    }

    errno = -err;
    return -1;
}

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

    for (size_t i = 0; i < FS_MAX_MOUNTPOINT; i++) {
        size_t prefix_length = strlen(mountpoints[i].dir);
        if (prefix_length > longest_length && strncmp(path, mountpoints[i].dir, prefix_length) == 0) {
            longest_match = &mountpoints[i];
            longest_length = prefix_length;
        }
    }
    return longest_match;
}

static bool is_valid_file_descriptor(int fildes) {
    if (fildes <= STDIO_FILNO_MAX || (int)max_file_descriptor <= FILENO_INDEX(fildes))
        return false;
    else
        return true;
}

int fs_format(filesystem_t *fs, blockdevice_t *device) {
    if (!device->is_initialized) {
        int err = device->init(device);
        if (err != BD_ERROR_OK) {
            return _error_remap(err);
        }
    }
    return fs->format(fs, device);
}

int fs_mount(const char *dir, filesystem_t *fs, blockdevice_t *device) {
    if (!device->is_initialized) {
        int err = device->init(device);
        if (err)
            return _error_remap(err);
    }

    int err = fs->mount(fs, device, false);
    if (err) {
        return _error_remap(err);
    }

    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);
    for (size_t i = 0; i < FS_MAX_MOUNTPOINT; i++) {
        if (mountpoints[i].filesystem == NULL) {
            mountpoints[i].filesystem = fs;
            mountpoints[i].device = device;
            mountpoints[i].dir = strdup(dir);
            recursive_mutex_exit(&_mutex);
            return _error_remap(0);
        }
    }
    recursive_mutex_exit(&_mutex);
    return _error_remap(-EFAULT);
}

int fs_unmount(const char *path) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    filesystem_t *fs = mp->filesystem;
    int err = fs->unmount(fs);
    if (err) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(err);
    }

    mp->filesystem = NULL;
    mp->device = NULL;
    free((char *)mp->dir);

    recursive_mutex_exit(&_mutex);
    return _error_remap(0);
}

int fs_reformat(const char *path) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    filesystem_t *fs = mp->filesystem;
    blockdevice_t *device = mp->device;

    int err = fs->unmount(fs);
    if (err) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(err);
    }
    err = fs->format(fs, device);
    if (err) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(err);
    }
    err = fs->mount(fs, device, false);

    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int fs_info(const char *path, filesystem_t **fs, blockdevice_t **device) {
    (void)fs;
    (void)device;

    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    *fs = mp->filesystem;
    *device = mp->device;

    recursive_mutex_exit(&_mutex);
    return _error_remap(0);
}

int _unlink(const char *path) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    int err = fs->remove(fs, entity_path);
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int rename(const char *old, const char *new) {
    // TODO: Check if old and new are the same filesystem
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);
    mountpoint_t *mp = find_mountpoint(old);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *old_entity_path = remove_prefix(old, mp->dir);
    const char *new_entity_path = remove_prefix(new, mp->dir);

    filesystem_t *fs = mp->filesystem;
    int err = fs->rename(fs, old_entity_path, new_entity_path);
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int mkdir(const char *path, mode_t mode) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    int err = fs->mkdir(fs, entity_path, mode);
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int rmdir(const char *path) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    int err = fs->rmdir(fs, entity_path);
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int _stat(const char *path, struct stat *st) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);
    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    filesystem_t *fs = mp->filesystem;
    int err = fs->stat(fs, entity_path, st);
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

int _fstat(int fildes, struct stat *st) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (fildes == STDIN_FILENO || fildes == STDOUT_FILENO || fildes == STDERR_FILENO) {
        recursive_mutex_exit(&_mutex);
        st->st_size = 0;
        st->st_mode = S_IFCHR;
        return _error_remap(0);
    }
    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }

    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }

    off_t size = 0;
    if (fs->type != FILESYSTEM_TYPE_FAT) {
        off_t current = fs->file_tell(fs, file);
        size = fs->file_seek(fs, file, 0, SEEK_END);
        off_t err = fs->file_seek(fs, file, current, SEEK_SET);
        if (current != err) {
            recursive_mutex_exit(&_mutex);
            return _error_remap(err);
        }
    } else {
        /* NOTE: Support for different behaviour of FatFs from POSIX
         *
         * FatFs has a problem where f_size() reports a larger than actual file size when
         * moved to a position beyond the f_lseek() file size; POSIX reports the actual size
         * written to the file, not the seek position.
         */
        const char *path = file_descriptor[FILENO_INDEX(fildes)].path;
        mountpoint_t *mp = find_mountpoint(path);
        if (mp == NULL) {
            recursive_mutex_exit(&_mutex);
            return _error_remap(-ENOENT);
        }
        const char *entity_path = remove_prefix(path, mp->dir);
        filesystem_t *fs = mp->filesystem;

        struct stat finfo = {0};
        int err = fs->stat(fs, entity_path, &finfo);
        if (err != 0) {
            recursive_mutex_exit(&_mutex);
            return _error_remap(err);
        }
        size = finfo.st_size;
    }
    recursive_mutex_exit(&_mutex);

    st->st_size = size;
    st->st_mode = S_IFREG;
    return _error_remap(0);
}

static int _assign_file_descriptor() {
    int fd = -1;
    if (max_file_descriptor == 0) {
        max_file_descriptor = 2;
        file_descriptor = calloc(max_file_descriptor, sizeof(file_descriptor_t));
        if (file_descriptor == NULL) {
            printf("_open: Out of memory\n");
            return -1;
        }
    }
    for (size_t i = 0; i < max_file_descriptor; i++) {
        if (file_descriptor[i].filesystem == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        // Extend the management array
        size_t last_max = max_file_descriptor;
        max_file_descriptor *= 2;
        file_descriptor = realloc(file_descriptor, sizeof(file_descriptor_t) * max_file_descriptor);
        if (file_descriptor == NULL) {
            printf("_open: Out of memory\n");
            return -1;
        }
        for (size_t i = last_max; i < max_file_descriptor; i++) {
            file_descriptor[i].filesystem = NULL;
            file_descriptor[i].file = NULL;
        }
        fd = last_max;
    }
    return FILENO_VALUE(fd);
}

static int _assign_dir_descriptor(void) {
    int fd = -1;
    if (max_dir_descriptor == 0) {
        max_dir_descriptor = 2;
        dir_descriptor = calloc(max_dir_descriptor, sizeof(dir_descriptor_t));
        if (dir_descriptor == NULL) {
            printf("_opendir: Out of memory\n");
            return -1;
        }
    }
    for (size_t i = 0; i < max_dir_descriptor; i++) {
        if (dir_descriptor[i].filesystem == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        // Extend the management array
        size_t last_max = max_dir_descriptor;
        max_dir_descriptor *= 2;
        dir_descriptor = realloc(dir_descriptor, sizeof(dir_descriptor_t) * max_dir_descriptor);
        if (dir_descriptor == NULL) {
            printf("_opendir: Out of memory\n");
            return -1;
        }
        for (size_t i = last_max; i < max_dir_descriptor; i++) {
            dir_descriptor[i].filesystem = NULL;
            dir_descriptor[i].dir = NULL;
        }
        fd = (int)last_max;
    }
    return fd;
}

int _open(const char *path, int oflags, ...) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOENT);
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    // find file descriptor
    int fd = _assign_file_descriptor();
    if (fd == -1) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENFILE);
    }

    filesystem_t *fs = mp->filesystem;
    fs_file_t *file = file_descriptor[FILENO_INDEX(fd)].file = calloc(1, sizeof(fs_file_t));
    if (file == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-ENOMEM);
    }

    int err = fs->file_open(fs, file, entity_path, oflags);
    if (err < 0) {
        free(file);
        file_descriptor[FILENO_INDEX(fd)].file = NULL;
        recursive_mutex_exit(&_mutex);
        return _error_remap(err);
    }
    file_descriptor[FILENO_INDEX(fd)].filesystem = fs;
    strncpy(file_descriptor[FILENO_INDEX(fd)].path, path, PATH_MAX);

    recursive_mutex_exit(&_mutex);

    return _error_remap(fd);
}

int _close(int fildes) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (!is_valid_file_descriptor(fildes)) {
        printf("_close error fildes=%d\n", fildes);
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    int err = fs->file_close(fs, file);
    free(file);
    file_descriptor[FILENO_INDEX(fildes)].filesystem = NULL;
    file_descriptor[FILENO_INDEX(fildes)].file = NULL;
    file_descriptor[FILENO_INDEX(fildes)].path[0] = '\0';

    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}


extern void stdio_flush(void);  // from pico_stdio/stcio.c

// _write to STDOUT_FILENO and STDERR_FILENO falls back to `putchar` in the pico_stdio library
static size_t pico_stdio_fallback_write(const void *buf, size_t nbyte) {
    const char *out = buf;
    for (size_t i = 0; i < nbyte; i++) {
        putchar(out[i]);
    }
    stdio_flush();
    return nbyte;
}

// _read to STDIN_FILENO fall back to `getchar` in the pico_stdio library
static size_t pico_stdio_fallback_read(void *buf, size_t nbyte) {
    (void)buf;
    (void)nbyte;

    uint8_t *in = buf;
    in[0] = getchar();
    return 1;
}

ssize_t _write(int fildes, const void *buf, size_t nbyte) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (fildes == STDOUT_FILENO || fildes == STDERR_FILENO) {
        pico_stdio_fallback_write(buf, nbyte);
        recursive_mutex_exit(&_mutex);
        return (ssize_t)nbyte;
    }
    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    recursive_mutex_exit(&_mutex);

    ssize_t size = fs->file_write(fs, file, buf, nbyte);

    return _error_remap(size);
}

ssize_t _read(int fildes, void *buf, size_t nbyte) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (fildes == STDIN_FILENO) {
        size_t read_bytes = pico_stdio_fallback_read(buf, nbyte);
        recursive_mutex_exit(&_mutex);
        return read_bytes;
    }
    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    recursive_mutex_exit(&_mutex);

    ssize_t size = fs->file_read(fs, file, buf, nbyte);

    return _error_remap(size);
}

off_t _lseek(int fildes, off_t offset, int whence) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }

    off_t pos = fs->file_seek(fs, file, offset, whence);
    recursive_mutex_exit(&_mutex);

    return _error_remap(pos);
}

off_t _ftello_r(struct _reent *ptr, register FILE *fp) {
    (void)ptr;
    int fildes = fp->_file;
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }

    off_t pos = fs->file_tell(fs, file);
    recursive_mutex_exit(&_mutex);

    return _error_remap(pos);
}

int ftruncate(int fildes, off_t length) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    if (!is_valid_file_descriptor(fildes)) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    fs_file_t *file = file_descriptor[FILENO_INDEX(fildes)].file;
    filesystem_t *fs = file_descriptor[FILENO_INDEX(fildes)].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }

    int err = fs->file_truncate(fs, file, length);
    recursive_mutex_exit(&_mutex);

    return _error_remap(err);
}

DIR *opendir(const char *path) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    mountpoint_t *mp = find_mountpoint(path);
    if (mp == NULL) {
        _error_remap(-ENOENT);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }
    const char *entity_path = remove_prefix(path, mp->dir);
    // find dir descriptor
    int fd = _assign_dir_descriptor();
    if (fd == -1) {
        _error_remap(-ENFILE);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }

    fs_dir_t *dir = dir_descriptor[fd].dir = calloc(1, sizeof(fs_dir_t));
    if (dir == NULL) {
        _error_remap(-ENOMEM);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }
    filesystem_t *fs = mp->filesystem;
    int err = fs->dir_open(fs, dir, entity_path);
    if (err != 0) {
        free(dir);
        dir_descriptor[fd].dir = NULL;
        _error_remap(err);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }
    dir_descriptor[fd].filesystem = fs;
    dir->fd = fd;
    recursive_mutex_exit(&_mutex);

    return dir;
}

int closedir(DIR *dir) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    int fd = dir->fd;
    fs_dir_t *_dir = dir_descriptor[dir->fd].dir;
    filesystem_t *fs = dir_descriptor[dir->fd].filesystem;
    if (fs == NULL) {
        recursive_mutex_exit(&_mutex);
        return _error_remap(-EBADF);
    }
    int err = fs->dir_close(fs, _dir);
    dir_descriptor[fd].filesystem = NULL;
    free(dir_descriptor[fd].dir);
    dir_descriptor[fd].dir = NULL;
    recursive_mutex_exit(&_mutex);
    return _error_remap(err);
}

struct dirent *readdir(DIR *dir) {
    auto_init_recursive_mutex(_mutex);
    recursive_mutex_enter_blocking(&_mutex);

    fs_dir_t *_dir = dir_descriptor[dir->fd].dir;
    filesystem_t *fs = dir_descriptor[dir->fd].filesystem;
    if (fs == NULL) {
        _error_remap(-EBADF);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }
    memset(&_dir->current, 0, sizeof(_dir->current));
    int err = fs->dir_read(fs, _dir, &_dir->current);
    if (err == 0) {
        recursive_mutex_exit(&_mutex);
        return &_dir->current;
    } else if (err == -ENOENT) {
        memset(&_dir->current, 0, sizeof(_dir->current));
        _error_remap(0);
        recursive_mutex_exit(&_mutex);
        return NULL;
    } else {
        memset(&_dir->current, 0, sizeof(_dir->current));
        _error_remap(err);
        recursive_mutex_exit(&_mutex);
        return NULL;
    }
}

char *fs_strerror(int errnum) {
    if (errnum > 5000) {
        // SD blockdevice error
        const char *str = "";
        switch (errnum) {
        case 5001:
            str = "operation would block";
            break;
        case 5002:
            str = "unsupported operation";
            break;
        case 5003:
            str = "invalid parameter";
            break;
        case 5004:
            str = "uninitialized";
            break;
        case 5005:
            str = "device is missing or not connected";
            break;
        case 5006:
            str = "write protected";
            break;
        case 5007:
            str = "unusable card";
            break;
        case 5008:
            str = "No response from device";
            break;
        case 5009:
            str = "CRC error";
            break;
        case 5010:
            str = "Erase error: reset/sequence";
            break;
        case 5011:
            str = "Write error: !SPI_DATA_ACCEPTED";
            break;
        default:
            break;
        }
        return (char *)str;
    } else if (errnum > 4000) {
        // On-board flash blockdevice error
        const char *str = "";
        switch (errnum) {
        case 4001:
            str = "operation timeout";
            break;
        case 4002:
            str = "safe execution is not possible";
            break;
        case 4003:
            str = "method fails due to dynamic resource exhaustion";
            break;
        default:
            break;
        }
        return (char *)str;
    } else {
        return strerror(errnum);
    }
}

#if defined(PICO_FS_AUTO_INIT)
void __attribute__((constructor)) pre_main(void) {
    fs_init();
}
#endif
