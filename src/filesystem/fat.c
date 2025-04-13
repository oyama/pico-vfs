/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pico/mutex.h>
#include "blockdevice/blockdevice.h"
#include "filesystem/fat.h"
#include "ff.h"
#include "diskio.h"


typedef struct {
    FIL file;
} fat_file_t;

typedef struct {
    FATFS fatfs;
    int id;
    mutex_t _mutex;
    mutex_t _mutex_format;
} filesystem_fat_context_t;

static const char FILESYSTEM_NAME[] = "FAT";
static blockdevice_t *_ffs[FF_VOLUMES] = {0};

static int fat_error_remap(FRESULT res) {
    switch (res) {
    case FR_OK:                   // (0) Succeeded
        return 0;
    case FR_DISK_ERR:             // (1) A hard error occurred in the low level disk I/O layer
        return -EIO;
    case FR_INT_ERR:              // (2) Assertion failed
        return -1;
    case FR_NOT_READY:            // (3) The physical drive cannot work
        return -EIO;
    case FR_NO_FILE:              // (4) Could not find the file
        return -ENOENT;
    case FR_NO_PATH:              // (5) Could not find the path
        return -ENOTDIR;
    case FR_INVALID_NAME:         // (6) The path name format is invalid
        return -EINVAL;
    case FR_DENIED:               // (7) Access denied due to prohibited access or directory full
        return -EACCES;
    case FR_EXIST:                // (8) Access denied due to prohibited access
        return -EEXIST;
    case FR_INVALID_OBJECT:       // (9) The file/directory object is invalid
        return -EBADF;
    case FR_WRITE_PROTECTED:      // (10) The physical drive is write protected
        return -EACCES;
    case FR_INVALID_DRIVE:        // (11) The logical drive number is invalid
        return -ENODEV;
    case FR_NOT_ENABLED:          // (12) The volume has no work area
        return -ENODEV;
    case FR_NO_FILESYSTEM:        // (13) There is no valid FAT volume
        return -EINVAL;
    case FR_MKFS_ABORTED:         // (14) The f_mkfs() aborted due to any problem
        return -EIO;
    case FR_TIMEOUT:              // (15) Could not get a grant to access the volume within defined period
        return -ETIMEDOUT;
    case FR_LOCKED:               // (16) The operation is rejected according to the file sharing policy
        return -EBUSY;
    case FR_NOT_ENOUGH_CORE:      // (17) LFN working buffer could not be allocated
        return -ENOMEM;
    case FR_TOO_MANY_OPEN_FILES:  // (18) Number of open files > FF_FS_LOCK
        return -ENFILE;
    case FR_INVALID_PARAMETER:    // (19) Given parameter is invalid
        return -EINVAL;
    default:
        return -res;
    }
}

static inline void debug_if(int condition, const char *format, ...) {
    if (condition) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

DSTATUS disk_initialize(BYTE pdrv) {
    debug_if(FFS_DBG, "disk_initialize on pdrv [%d]\n", pdrv);
    return (DSTATUS)_ffs[pdrv]->init(_ffs[pdrv]);
}

static WORD disk_get_sector_size(BYTE pdrv) {
    size_t sector_size = _ffs[(int)pdrv]->erase_size;

    WORD ssize = sector_size;
    if (ssize < 512) {
        ssize = 512;
    }
    return ssize;
}

static DWORD disk_get_sector_count(BYTE pdrv) {
    DWORD scount = (_ffs[(int)pdrv]->size(_ffs[(int)pdrv])) / disk_get_sector_size(pdrv);
    return scount;
}

DSTATUS disk_status(BYTE pdrv) {
    debug_if(FFS_DBG, "disk_status on pdrv [%d]\n", pdrv);
    return RES_OK;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    DWORD ssize = disk_get_sector_size(pdrv);
    bd_size_t addr = (bd_size_t)sector * ssize;
    bd_size_t size = count * ssize;
    int err = _ffs[(int)pdrv]->read(_ffs[(int)pdrv], (const void *)buff, addr, size);
    return err ? RES_PARERR : RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    debug_if(FFS_DBG, "disk_write [%d]\n", pdrv);
    DWORD ssize = disk_get_sector_size(pdrv);
    bd_size_t addr = (bd_size_t)sector * ssize;
    bd_size_t size = count * ssize;

    int err = _ffs[pdrv]->erase(_ffs[pdrv], addr, size);
    if (err) {
        return RES_PARERR;
    }

    err = _ffs[pdrv]->program(_ffs[pdrv], buff, addr, size);
    if (err) {
        return RES_PARERR;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    debug_if(FFS_DBG, "disk_ioctl(%d)\n", cmd);
    switch (cmd) {
    case CTRL_SYNC:
        if (_ffs[pdrv] == NULL) {
            return RES_NOTRDY;
        } else {
            return RES_OK;
        }
    case GET_SECTOR_COUNT:
        if (_ffs[pdrv] == NULL) {
            return RES_NOTRDY;
        } else {
            *((DWORD *)buff) = disk_get_sector_count(pdrv);
            return RES_OK;
        }
    case GET_SECTOR_SIZE:
        if (_ffs[pdrv] == NULL) {
            return RES_NOTRDY;
        } else {
            *((WORD *)buff) = disk_get_sector_size(pdrv);
            return RES_OK;
        }
    case GET_BLOCK_SIZE:
        *((DWORD *)buff) = 1; // default when not known
        return RES_OK;
    case CTRL_TRIM:
        if (_ffs[pdrv] == NULL) {
            return RES_NOTRDY;
        } else {
            DWORD *sectors = (DWORD *)buff;
            DWORD ssize = disk_get_sector_size(pdrv);
            bd_size_t addr = (bd_size_t)sectors[0] * ssize;
            bd_size_t size = (bd_size_t)(sectors[1] - sectors[0] + 1) * ssize;
            int err = _ffs[pdrv]->trim(_ffs[pdrv], addr, size);
            return err ? RES_PARERR : RES_OK;
        }
    }
    return RES_PARERR;
}

DWORD get_fattime(void) {
    time_t rawtime;
    time(&rawtime);
    struct tm *ptm = localtime(&rawtime);
    return (DWORD)(ptm->tm_year - 80) << 25
           | (DWORD)(ptm->tm_mon + 1) << 21
           | (DWORD)(ptm->tm_mday) << 16
           | (DWORD)(ptm->tm_hour) << 11
           | (DWORD)(ptm->tm_min) << 5
           | (DWORD)(ptm->tm_sec / 2);
}


static int mount(filesystem_t *fs, blockdevice_t *device, bool pending) {
    filesystem_fat_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex);

    char _fsid[3] = {0};
    for (size_t i = 0; i < FF_VOLUMES; i++) {
        if (_ffs[i] == NULL) {
            context->id = i;
            _ffs[i] = device;
            _fsid[0] = '0' + i;
            _fsid[1] = ':';
            _fsid[2] = '\0';
            FRESULT res = f_mount(&context->fatfs, _fsid, !pending);
            mutex_exit(&context->_mutex);
            return fat_error_remap(res);
        }
    }
    mutex_exit(&context->_mutex);
    return -ENOMEM;
}

static int unmount(filesystem_t *fs) {
    filesystem_fat_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex);

    char _fsid[3] = "0:";
    _fsid[0] = '0' + context->id;

    FRESULT res = f_mount(NULL, _fsid, 0);
    _ffs[context->id] = NULL;

    mutex_exit(&context->_mutex);
    return fat_error_remap(res);
}

static int format(filesystem_t *fs, blockdevice_t *device) {
    filesystem_fat_context_t *context = fs->context;
    mutex_enter_blocking(&context->_mutex_format);

    if (!device->is_initialized) {
        int err = device->init(device);
        if (err) {
            mutex_exit(&context->_mutex_format);
            return err;
        }
    }

    // erase first handful of blocks
    bd_size_t header = 2 * device->erase_size;
    int err = device->erase(device, 0, header);
    if (err) {
        mutex_exit(&context->_mutex_format);
        return err;
    }

    size_t program_size = device->program_size;
    void *buffer = malloc(program_size);
    if (!buffer) {
        mutex_exit(&context->_mutex_format);
        return -ENOMEM;
    }
    memset(buffer, 0xFF, program_size);
    for (size_t i = 0; i < header; i += program_size) {
        err = device->program(device, buffer, i, program_size);
        if (err) {
            free(buffer);
            mutex_exit(&context->_mutex_format);
            return err;
        }
    }
    free(buffer);

    // trim entire device to indicate it is unneeded
    err = device->trim(device, 0, device->size(device));
    if (err) {
        mutex_exit(&context->_mutex_format);
        return err;
    }

    err = fs->mount(fs, device, true);
    if (err) {
        mutex_exit(&context->_mutex_format);
        return err;
    }

    MKFS_PARM opt;
    opt.fmt = (FM_ANY | FM_SFD);
    opt.n_fat = 0;
    opt.align = 0U;
    opt.n_root = 0U;
    opt.au_size = 0; // (DWORD)cluster_size;

    char id[3] = "0:";
    id[0] = '0' + context->id;

    FRESULT res = f_mkfs((const TCHAR *)id, &opt, NULL, FF_MAX_SS);

    if (res != FR_OK) {
        fs->unmount(fs);
        mutex_exit(&context->_mutex_format);
        return fat_error_remap(res);
    }

    err = fs->unmount(fs);
    if (err) {
        mutex_exit(&context->_mutex_format);
        return res;
    }

    mutex_exit(&context->_mutex_format);
    return 0;
}

static const char *fat_path_prefix(char *dist, int id, const char *path) {
    if (id == 0) {
        strcpy(dist, path);
        return (const char *)dist;
    }

    dist[0] = '0' + id;
    dist[1] = ':';
    strcpy(dist + strlen("0:"), path);
    return (const char *)dist;
}

static int file_remove(filesystem_t *fs, const char *path) {
    filesystem_fat_context_t *context = fs->context;

    char fpath[PATH_MAX];
    fat_path_prefix(fpath, context->id, path);

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_unlink(fpath);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_unlink() failed: %d\n", res);
        if (res == FR_DENIED)
            return -ENOTEMPTY;
    }
    return fat_error_remap(res);
}

static int file_rename(filesystem_t *fs, const char *oldpath, const char *newpath) {
    filesystem_fat_context_t *context = fs->context;
    char oldfpath[PATH_MAX];
    char newfpath[PATH_MAX];
    fat_path_prefix(oldfpath, context->id, oldpath);
    fat_path_prefix(newfpath, context->id, newpath);

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_rename(oldfpath, newfpath);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_rename() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

static int file_mkdir(filesystem_t *fs, const char *path, mode_t mode) {
    (void)mode;
    filesystem_fat_context_t *context = fs->context;
    char fpath[PATH_MAX];
    fat_path_prefix(fpath, context->id, path);

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_mkdir(fpath);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_mkdir() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

static int file_rmdir(filesystem_t *fs, const char *path) {
    filesystem_fat_context_t *context = fs->context;
    char fpath[PATH_MAX];
    fat_path_prefix(fpath, context->id, path);

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_unlink(fpath);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_unlink() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

static int file_stat(filesystem_t *fs, const char *path, struct stat *st) {
    filesystem_fat_context_t *context = fs->context;
    char fpath[PATH_MAX];
    fat_path_prefix(fpath, context->id, path);
    FILINFO f = {0};

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_stat(fpath, &f);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        return fat_error_remap(res);
    }
    st->st_size = f.fsize;
    st->st_mode = 0;
    st->st_mode |= (f.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_mode |= (f.fattrib & AM_RDO) ?
                   (S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) :
                   (S_IRWXU | S_IRWXG | S_IRWXO);
    return 0;
}

static int file_open(filesystem_t *fs, fs_file_t *file, const char *path, int flags) {
    BYTE open_mode;
    if (flags & O_RDWR)
        open_mode = FA_READ | FA_WRITE;
    else if (flags & O_WRONLY)
        open_mode = FA_WRITE;
    else
        open_mode = FA_READ;
    if (flags & O_CREAT) {
        if (flags & O_TRUNC)
            open_mode |= FA_CREATE_ALWAYS;
        else
            open_mode |= FA_OPEN_ALWAYS;
    }
    if (flags & O_APPEND)
        open_mode |= FA_OPEN_APPEND;

    char fpath[PATH_MAX];
    filesystem_fat_context_t *context = fs->context;
    fat_path_prefix(fpath, context->id, path);
    fat_file_t *fat_file = file->context = calloc(1, sizeof(fat_file_t));
    if (fat_file == NULL) {
        fprintf(stderr, "file_open: Out of memory\n");
        return -ENOMEM;
    }

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_open(&fat_file->file, fpath, open_mode);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_open('w') failed: %d\n", res);
        return fat_error_remap(res);
    }
    return 0;
}

static int file_close(filesystem_t *fs, fs_file_t *file) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_close(&fat_file->file);
    mutex_exit(&context->_mutex);

    free(file->context);
    file->context = NULL;
    return fat_error_remap(res);
}

static ssize_t file_write(filesystem_t *fs, fs_file_t *file, const void *buffer, size_t size) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    UINT n;

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_write(&(fat_file->file), buffer, size, &n);
    if (res != FR_OK) {
        mutex_exit(&context->_mutex);
        debug_if(FFS_DBG, "f_write() failed: %d", res);
        return fat_error_remap(res);
    }
    res = f_sync(&fat_file->file);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_write() failed: %d", res);
        return fat_error_remap(res);
    }
    return n;
}

static ssize_t file_read(filesystem_t *fs, fs_file_t *file, void *buffer, size_t size) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    UINT n;

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_read(&fat_file->file, buffer, size, &n);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_read() failed: %d\n", res);
        return fat_error_remap(res);
    }
    return n;
}

static int file_sync(filesystem_t *fs, fs_file_t *file) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_sync(&fat_file->file);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_sync() failed: %d\n", res);
    }
    return fat_error_remap(res);
}

static off_t file_seek(filesystem_t *fs, fs_file_t *file, off_t offset, int whence) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    if (whence == SEEK_END)
        offset += f_size(&fat_file->file);
    else if (whence == SEEK_CUR)
        offset += f_tell(&fat_file->file);

    FRESULT res = res = f_lseek(&fat_file->file, offset);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "lseek failed: %d\n", res);
        return fat_error_remap(res);
    }
    return (off_t)fat_file->file.fptr;
}

static off_t file_tell(filesystem_t *fs, fs_file_t *file) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = f_tell(&fat_file->file);
    mutex_exit(&context->_mutex);

    return res;
}

static off_t file_size(filesystem_t *fs, fs_file_t *file) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    off_t res = f_size(&fat_file->file);
    mutex_exit(&context->_mutex);

    return res;
}

static int file_truncate(filesystem_t *fs, fs_file_t *file, off_t length) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;
    fat_file_t *fat_file = file->context;

    mutex_enter_blocking(&context->_mutex);
    FSIZE_t old_offset = f_tell(&fat_file->file);
    FRESULT res = f_lseek(&fat_file->file, length);
    if (res) {
        mutex_exit(&context->_mutex);
        return fat_error_remap(res);
    }
    res = f_truncate(&fat_file->file);
    if (res) {
        mutex_exit(&context->_mutex);
        return fat_error_remap(res);
    }
    res = f_lseek(&fat_file->file, old_offset);
    mutex_exit(&context->_mutex);

    if (res) {
        return fat_error_remap(res);
    }
    return 0;
}

static int dir_open(filesystem_t *fs, fs_dir_t *dir, const char *path) {
    filesystem_fat_context_t *context = fs->context;
    char fpath[PATH_MAX];
    fat_path_prefix(fpath, context->id, path);

    FATFS_DIR *dh = calloc(1, sizeof(FATFS_DIR));
    if (dh == NULL) {
        fprintf(stderr, "dir_open: Out of memory\n");
        return -ENOMEM;
    }

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_opendir(dh, fpath);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        debug_if(FFS_DBG, "f_opendir() failed: %d\n", res);
        free(dh);
        return fat_error_remap(res);
    }
    dir->context = dh;
    dir->fd = -1;
    return 0;
}

static int dir_close(filesystem_t *fs, fs_dir_t *dir) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;

    FATFS_DIR *dh = (FATFS_DIR *)dir->context;

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_closedir(dh);
    mutex_exit(&context->_mutex);

    free(dh);
    return fat_error_remap(res);
}

static int dir_read(filesystem_t *fs, fs_dir_t *dir, struct dirent *ent) {
    (void)fs;
    filesystem_fat_context_t *context = fs->context;

    FATFS_DIR *dh = (FATFS_DIR *)dir->context;
    FILINFO finfo = {0};

    mutex_enter_blocking(&context->_mutex);
    FRESULT res = f_readdir(dh, &finfo);
    mutex_exit(&context->_mutex);

    if (res != FR_OK) {
        return fat_error_remap(res);
    } else if (finfo.fname[0] == 0) {
        return -ENOENT;
    }
    ent->d_type = (finfo.fattrib & AM_DIR) ? DT_DIR : DT_REG;
    strncpy(ent->d_name, finfo.fname, FF_LFN_BUF);
    ent->d_name[FF_LFN_BUF] = '\0';
    return 0;
}


filesystem_t *filesystem_fat_create() {
    filesystem_t *fs = calloc(1, sizeof(filesystem_t));
    if (fs == NULL) {
        fprintf(stderr, "filesystem_fat_create: Out of memory\n");
        return NULL;
    }
    fs->type = FILESYSTEM_TYPE_FAT;
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
    filesystem_fat_context_t *context = calloc(1, sizeof(filesystem_fat_context_t));
    if (context == NULL) {
        fprintf(stderr, "filesystem_fat_create: Out of memory\n");
        free(fs);
        return NULL;
    }
    context->id = -1;
    mutex_init(&context->_mutex);
    mutex_init(&context->_mutex_format);

    fs->context = context;
    return fs;
}

void filesystem_fat_free(filesystem_t *fs) {
    free(fs->context);
    fs->context = NULL;
    free(fs);
    fs = NULL;
}
