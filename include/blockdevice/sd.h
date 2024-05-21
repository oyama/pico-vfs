/*
 * Copyright 2024, Hiroyuki OYAMA. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <hardware/spi.h>
#include <hardware/clocks.h>
#include <pico/stdlib.h>
#include <errno.h>

#include "blockdevice/blockdevice.h"


#define CONF_SD_INIT_FREQUENCY    (10 * 1000 * 1000)
#define CONF_SD_TRX_FREQUENCY     (24 * MHZ)

typedef struct {
    spi_inst_t *spi_inst;
    uint8_t mosi; // PICO_DEFAULT_SPI_TX_PIN  pin 19
    uint8_t miso; // PICO_DEFAULT_SPI_RX_PIN  pin 16
    uint8_t sclk; // PICO_DEFAULT_SPI_SCK_PIN pin 18
    uint8_t cs;   // PICO_DEFAULT_SPI_CSN_PIN pin 17
    uint32_t hz;  // CONF_SD_TRX_FREQUENCY
    bool enable_crc;
    uint8_t card_type;
    bool is_initialized;
    size_t block_size;
    size_t erase_size;
    size_t total_sectors;
} blockdevice_sd_config_t;


blockdevice_t *blockdevice_sd_create(spi_inst_t *spi_inst,
                                     uint8_t mosi,
                                     uint8_t miso,
                                     uint8_t sckl,
                                     uint8_t cs,
                                     uint32_t hz,
                                     bool enable_crc);
void blockdevice_sd_free(blockdevice_t *device);
