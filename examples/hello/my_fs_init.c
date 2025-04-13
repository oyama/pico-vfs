/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"


bool fs_init(void) {
    printf("Initialize custom file system\n");

    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * MHZ,
                                              false);
    filesystem_t *lfs = filesystem_littlefs_create(500, 16);
    filesystem_t *fat = filesystem_fat_create();

    // Mount littlefs on-board flash to `/`
    int err = fs_mount("/", lfs, flash);
    if (err == -1) {
        printf("format / with littlefs\n");
        err = fs_format(lfs, flash);
        if (err == -1) {
            printf("fs_format error: %s\n", strerror(errno));
            return false;
        }
        err = fs_mount("/", lfs, flash);
        if (err == -1) {
            printf("fs_mount error: %s\n", strerror(errno));
            return false;
        }
    }

    // Mount FAT SD card `/sd`
    err = fs_mount("/sd", fat, sd);
    if (err == -1) {
        printf("format /sd with FAT\n");
        err = fs_format(fat, sd);
        if (err == -1) {
            printf("fs_format error: %s\n", strerror(errno));
            return false;
        }
        err = fs_mount("/sd", fat, sd);
        if (err == -1) {
            printf("fs_mount error: %s\n", strerror(errno));
            return false;
        }
    }

    return err == 0;
}
