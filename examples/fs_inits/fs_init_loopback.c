/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/loopback.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define FLASH_LENGTH_ALL      0
#define LOOPBACK_FILE_SIZE    (640 * 1024)
#define LOOPBACK_BLOCK_SIZE   512

bool fs_init(void) {
    // mount onboard flash
    blockdevice_t *device = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, FLASH_LENGTH_ALL);
    filesystem_t *fs = filesystem_littlefs_create(500, 16);
    int err = fs_mount("/flash", fs, device);
    if (err == -1) {
        printf("format /flash\n");
        err = fs_format(fs, device);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/flash", fs, device);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }

    // mount loopback device
    printf("fs_init FAT on loopback on littlefs\n");
    blockdevice_t *loopback = blockdevice_loopback_create("/flash/disk-image.dmg", LOOPBACK_FILE_SIZE, LOOPBACK_BLOCK_SIZE);
    filesystem_t *fat = filesystem_fat_create();
    err = fs_mount("/", fat, loopback);
    if (err == -1) {
        printf("format / with FAT\n");
        err = fs_format(fat, loopback);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/", fat, loopback);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }

    return true;
}
