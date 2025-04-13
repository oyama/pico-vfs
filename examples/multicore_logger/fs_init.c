/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"

bool fs_init(void) {
    printf("fs_init FAT on SD card\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * 1000 * 1000,
                                              true);
    filesystem_t *fat = filesystem_fat_create();
    int err = fs_mount("/sd", fat, sd);
    if (err == -1) {
        printf("format /sd with FAT\n");
        err = fs_format(fat, sd);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/sd", fat, sd);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }
    return true;
}
