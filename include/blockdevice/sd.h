/*
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

/** \defgroup blockdevice_sd blockdevice_sd
 *  \ingroup blockdevice
 *  \brief SPI-connected SD/MMC card block device
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <hardware/spi.h>
#include "blockdevice/blockdevice.h"


#define CONF_SD_INIT_FREQUENCY    (10 * 1000 * 1000)
#define CONF_SD_TRX_FREQUENCY     (24 * MHZ)

/*! \brief Create SD card block device with SPI
 * \ingroup blockdevice_sd
 *
 * Create a block device object for an SPI-connected SD or MMC card.
 *
 * \param spi_inst SPI instance, as defined in the pico-sdk hardware_spi library
 * \param mosi SPI Master Out Slave In(TX) pin
 * \param miso SPI Master In Slave Out(RX) pin
 * \param sckl SPI clock pin
 * \param cs SPI Chip select pin
 * \param hz SPI clock frequency (Hz)
 * \param enable_crc Boolean value to enable CRC on read/write
 * \return Block device object. Returnes NULL in case of failure.
 * \retval NULL Failed to create block device object.
 */
blockdevice_t *blockdevice_sd_create(spi_inst_t *spi_inst,
                                     uint8_t mosi,
                                     uint8_t miso,
                                     uint8_t sckl,
                                     uint8_t cs,
                                     uint32_t hz,
                                     bool enable_crc);

/*! \brief Release the SD card device.
 * \ingroup blockdevice_sd
 *
 * \param device Block device object.
 */
void blockdevice_sd_free(blockdevice_t *device);

#ifdef __cplusplus
}
#endif
