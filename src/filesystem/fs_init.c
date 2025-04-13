/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"


bool __attribute__((weak)) fs_init(void) {
    filesystem_t *fs = filesystem_littlefs_create(500, 16);
    blockdevice_t *device = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);

    int err = fs_mount("/", fs, device);
    if (err == -1) {
        err = fs->format(fs, device);
        if (err == -1) {
            return false;
        }
        err = fs_mount("/", fs, device);
    }
    return err == 0;
}
