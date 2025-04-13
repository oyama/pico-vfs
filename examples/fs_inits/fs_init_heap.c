/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/heap.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define HEAP_STORAGE_SIZE    (128 * 1024)

bool fs_init(void) {
    printf("fs_init FAT on Heap\n"); // Benchmarking is done at / due to capacity issues.
    blockdevice_t *heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    filesystem_t *fat = filesystem_fat_create();
    int err = fs_mount("/", fat, heap);
    if (err == -1) {
        printf("format / with FAT\n");
        err = fs_format(fat, heap);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/", fat, heap);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }

    // mount onboard flash
    blockdevice_t *device = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
    filesystem_t *fs = filesystem_littlefs_create(500, 16);
    err = fs_mount("/flash", fs, device);
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

    return true;
}
