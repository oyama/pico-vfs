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
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

bool fs_init(void) {
    printf("fs_init littlefs on SD card\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              false);
    filesystem_t *littlefs = filesystem_littlefs_create(500, 16);
    int err = fs_mount("/", littlefs, sd);
    if (err == -1) {
        printf("format / with littlefs\n");
        err = fs_format(littlefs, sd);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/", littlefs, sd);
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
