/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"

bool fs_init(void) {
    blockdevice_t *device = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
    filesystem_t *fs = filesystem_fat_create();

    printf("fs_init FAT on onboard flash\n");
    int err = fs_mount("/", fs, device);
    if (err == -1) {
        printf("format /\n");
        err = fs_format(fs, device);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/", fs, device);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }
    return true;
}
