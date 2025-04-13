/* Block device driver for SD and MMC cards via SPI
 *
 * Copyright 2024, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The source code was implemented with reference to the
 * [ARM Mbed OS](https://github.com/ARMmbed/mbed-os/blob/master/storage/blockdevice/COMPONENT_SD/source/SDBlockDevice.cpp);
 * ARM Embed OS is licensed under the Apache 2.0 licence.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <hardware/clocks.h>
#include <pico/mutex.h>
#include <pico/stdlib.h>
#include "blockdevice/sd.h"

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
    uint64_t total_sectors;
    mutex_t _mutex;
} blockdevice_sd_config_t;

#ifndef CONF_SD_CMD_TIMEOUT
#define CONF_SD_CMD_TIMEOUT                 5000   /*!< Timeout in ms for response */
#endif

#ifndef CONF_SD_CMD0_IDLE_STATE_RETRIES
#define CONF_SD_CMD0_IDLE_STATE_RETRIES   5  // Number of retries for sending CMD0
#endif

#define SD_COMMAND_TIMEOUT                CONF_SD_CMD_TIMEOUT
#define SD_CMD0_GO_IDLE_STATE_RETRIES     CONF_SD_CMD0_IDLE_STATE_RETRIES
#define SD_DBG                                   0      /*!< 1 - Enable debugging */

#define SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK        -5001  /*!< operation would block */
#define SD_BLOCK_DEVICE_ERROR_UNSUPPORTED        -5002  /*!< unsupported operation */
#define SD_BLOCK_DEVICE_ERROR_PARAMETER          -5003  /*!< invalid parameter */
#define SD_BLOCK_DEVICE_ERROR_NO_INIT            -5004  /*!< uninitialized */
#define SD_BLOCK_DEVICE_ERROR_NO_DEVICE          -5005  /*!< device is missing or not connected */
#define SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED    -5006  /*!< write protected */
#define SD_BLOCK_DEVICE_ERROR_UNUSABLE           -5007  /*!< unusable card */
#define SD_BLOCK_DEVICE_ERROR_NO_RESPONSE        -5008  /*!< No response from device */
#define SD_BLOCK_DEVICE_ERROR_CRC                -5009  /*!< CRC error */
#define SD_BLOCK_DEVICE_ERROR_ERASE              -5010  /*!< Erase error: reset/sequence */
#define SD_BLOCK_DEVICE_ERROR_WRITE              -5011  /*!< SPI Write error: !SPI_DATA_ACCEPTED */


#define BLOCK_SIZE_HC                            512    /*!< Block size supported for SD card is 512 bytes  */
#define SPI_CMD(x) (0x40 | (x & 0x3f))

/* R1 Response Format */
#define R1_NO_RESPONSE          (0xFF)
#define R1_RESPONSE_RECV        (0x80)
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

/* R3 Response : OCR Register */
#define OCR_HCS_CCS             (0x1 << 30)
#define OCR_LOW_VOLTAGE         (0x01 << 24)
#define OCR_3_3V                (0x1 << 20)

/* R7 response pattern for CMD8 */
#define CMD8_PATTERN             (0xAA)

/* Control Tokens   */
#define SPI_DATA_RESPONSE_MASK   (0x1F)
#define SPI_DATA_ACCEPTED        (0x05)
#define SPI_DATA_CRC_ERROR       (0x0B)
#define SPI_DATA_WRITE_ERROR     (0x0D)
#define SPI_START_BLOCK          (0xFE)      /*!< For Single Block Read/Write and Multiple Block Read */
#define SPI_START_BLK_MUL_WRITE  (0xFC)      /*!< Start Multi-block write */
#define SPI_STOP_TRAN            (0xFD)      /*!< Stop Multi-block write */

#define SPI_DATA_READ_ERROR_MASK (0xF)       /*!< Data Error Token: 4 LSB bits */
#define SPI_READ_ERROR           (0x1 << 0)  /*!< Error */
#define SPI_READ_ERROR_CC        (0x1 << 1)  /*!< CC Error*/
#define SPI_READ_ERROR_ECC_C     (0x1 << 2)  /*!< Card ECC failed */
#define SPI_READ_ERROR_OFR       (0x1 << 3)  /*!< Out of Range */


enum sdcard_type {
   SDCARD_NONE  = 0,
   SDCARD_V1    = 1,
   SDCARD_V2    = 2,
   SDCARD_V2HC  = 3,
   CARD_UNKNOWN = 4,
};

enum cmd_supported {
    CMD_NOT_SUPPORTED = -1,             /**< Command not supported error */
    CMD0_GO_IDLE_STATE = 0,             /**< Resets the SD Memory Card */
    CMD1_SEND_OP_COND = 1,              /**< Sends host capacity support */
    CMD6_SWITCH_FUNC = 6,               /**< Check and Switches card function */
    CMD8_SEND_IF_COND = 8,              /**< Supply voltage info */
    CMD9_SEND_CSD = 9,                  /**< Provides Card Specific data */
    CMD10_SEND_CID = 10,                /**< Provides Card Identification */
    CMD12_STOP_TRANSMISSION = 12,       /**< Forces the card to stop transmission */
    CMD13_SEND_STATUS = 13,             /**< Card responds with status */
    CMD16_SET_BLOCKLEN = 16,            /**< Length for SC card is set */
    CMD17_READ_SINGLE_BLOCK = 17,       /**< Read single block of data */
    CMD18_READ_MULTIPLE_BLOCK = 18,     /**< Card transfers data blocks to host until interrupted
                                             by a STOP_TRANSMISSION command */
    CMD24_WRITE_BLOCK = 24,             /**< Write single block of data */
    CMD25_WRITE_MULTIPLE_BLOCK = 25,    /**< Continuously writes blocks of data until
                                             'Stop Tran' token is sent */
    CMD27_PROGRAM_CSD = 27,             /**< Programming bits of CSD */
    CMD32_ERASE_WR_BLK_START_ADDR = 32, /**< Sets the address of the first write
                                             block to be erased. */
    CMD33_ERASE_WR_BLK_END_ADDR = 33,   /**< Sets the address of the last write
                                             block of the continuous range to be erased.*/
    CMD38_ERASE = 38,                   /**< Erases all previously selected write blocks */
    CMD55_APP_CMD = 55,                 /**< Extend to Applications specific commands */
    CMD56_GEN_CMD = 56,                 /**< General Purpose Command */
    CMD58_READ_OCR = 58,                /**< Read OCR register of card */
    CMD59_CRC_ON_OFF = 59,              /**< Turns the CRC option on or off*/
    // App Commands
    ACMD6_SET_BUS_WIDTH = 6,
    ACMD13_SD_STATUS = 13,
    ACMD22_SEND_NUM_WR_BLOCKS = 22,
    ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
    ACMD41_SD_SEND_OP_COND = 41,
    ACMD42_SET_CLR_CARD_DETECT = 42,
    ACMD51_SEND_SCR = 51,
};

#define PACKET_SIZE   6  // SD Packet size CMD+ARG+CRC

static const char DEVICE_NAME[] = "sd";
static const size_t block_size = 512;
static const uint8_t SPI_FILL_CHAR = 0xFF;

static const uint8_t CRC7_TABLE[256] = {
    0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36, 0x3F,
    0x48, 0x41, 0x5A, 0x53, 0x6C, 0x65, 0x7E, 0x77,
    0x19, 0x10, 0x0B, 0x02, 0x3D, 0x34, 0x2F, 0x26,
    0x51, 0x58, 0x43, 0x4A, 0x75, 0x7C, 0x67, 0x6E,
    0x32, 0x3B, 0x20, 0x29, 0x16, 0x1F, 0x04, 0x0D,
    0x7A, 0x73, 0x68, 0x61, 0x5E, 0x57, 0x4C, 0x45,
    0x2B, 0x22, 0x39, 0x30, 0x0F, 0x06, 0x1D, 0x14,
    0x63, 0x6A, 0x71, 0x78, 0x47, 0x4E, 0x55, 0x5C,
    0x64, 0x6D, 0x76, 0x7F, 0x40, 0x49, 0x52, 0x5B,
    0x2C, 0x25, 0x3E, 0x37, 0x08, 0x01, 0x1A, 0x13,
    0x7D, 0x74, 0x6F, 0x66, 0x59, 0x50, 0x4B, 0x42,
    0x35, 0x3C, 0x27, 0x2E, 0x11, 0x18, 0x03, 0x0A,
    0x56, 0x5F, 0x44, 0x4D, 0x72, 0x7B, 0x60, 0x69,
    0x1E, 0x17, 0x0C, 0x05, 0x3A, 0x33, 0x28, 0x21,
    0x4F, 0x46, 0x5D, 0x54, 0x6B, 0x62, 0x79, 0x70,
    0x07, 0x0E, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x38,
    0x41, 0x48, 0x53, 0x5A, 0x65, 0x6C, 0x77, 0x7E,
    0x09, 0x00, 0x1B, 0x12, 0x2D, 0x24, 0x3F, 0x36,
    0x58, 0x51, 0x4A, 0x43, 0x7C, 0x75, 0x6E, 0x67,
    0x10, 0x19, 0x02, 0x0B, 0x34, 0x3D, 0x26, 0x2F,
    0x73, 0x7A, 0x61, 0x68, 0x57, 0x5E, 0x45, 0x4C,
    0x3B, 0x32, 0x29, 0x20, 0x1F, 0x16, 0x0D, 0x04,
    0x6A, 0x63, 0x78, 0x71, 0x4E, 0x47, 0x5C, 0x55,
    0x22, 0x2B, 0x30, 0x39, 0x06, 0x0F, 0x14, 0x1D,
    0x25, 0x2C, 0x37, 0x3E, 0x01, 0x08, 0x13, 0x1A,
    0x6D, 0x64, 0x7F, 0x76, 0x49, 0x40, 0x5B, 0x52,
    0x3C, 0x35, 0x2E, 0x27, 0x18, 0x11, 0x0A, 0x03,
    0x74, 0x7D, 0x66, 0x6F, 0x50, 0x59, 0x42, 0x4B,
    0x17, 0x1E, 0x05, 0x0C, 0x33, 0x3A, 0x21, 0x28,
    0x5F, 0x56, 0x4D, 0x44, 0x7B, 0x72, 0x69, 0x60,
    0x0E, 0x07, 0x1C, 0x15, 0x2A, 0x23, 0x38, 0x31,
    0x46, 0x4F, 0x54, 0x5D, 0x62, 0x6B, 0x70, 0x79
};

static uint8_t _crc7(const void *buffer, size_t length) {
    const uint8_t *b = buffer;
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++)
        crc = CRC7_TABLE[(crc << 1) ^ b[i]];
    return crc;
}

static const uint16_t CRC16_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t _crc16(const void *buffer, size_t length) {
    const uint8_t *data = buffer;
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++)
        crc = (crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ data[i]) & 0x00FF];
    return crc;
}

static uint8_t _spi_write(void *_config, uint8_t data) {
    blockdevice_sd_config_t *config = _config;
    uint8_t *buffer = (uint8_t *)&data;
    spi_write_read_blocking(config->spi_inst, buffer, buffer, 1);
    return (uint8_t)*buffer;
}

static void _spi_wait(void *_config, size_t count) {
    blockdevice_sd_config_t *config = _config;
    for (size_t i = 0; i < count; i++) {
        spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, NULL, sizeof(SPI_FILL_CHAR));
    }
}

static void _spi_init(void *_config) {
    blockdevice_sd_config_t *config = _config;

    gpio_set_function(config->mosi, GPIO_FUNC_SPI);
    gpio_set_function(config->miso, GPIO_FUNC_SPI);
    gpio_set_function(config->sclk, GPIO_FUNC_SPI);
    gpio_init(config->cs);
    gpio_set_dir(config->cs, GPIO_OUT);
    gpio_pull_up(config->miso);
    gpio_set_drive_strength(config->mosi, 1);
    gpio_set_drive_strength(config->sclk, 1);
    spi_init(config->spi_inst, CONF_SD_INIT_FREQUENCY);
    spi_set_format(config->spi_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(config->cs, 1);
    bool old_cs = gpio_get(config->cs);
    _spi_wait(config, 10);
    gpio_put(config->cs, old_cs);
}

static void _preclock_then_select(void *_config) {
    blockdevice_sd_config_t *config = _config;
    spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, NULL, sizeof(SPI_FILL_CHAR));
    gpio_put(config->cs, 0);
}

static void _postclock_then_deselect(void *_config) {
    blockdevice_sd_config_t *config = _config;
    spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, NULL, sizeof(SPI_FILL_CHAR));
    gpio_put(config->cs, 1);
}

static inline void debug_if(int condition, const char *format, ...) {
    if (condition) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

static bool _wait_token(void *_config, uint8_t token) {
    blockdevice_sd_config_t *config = _config;

    uint32_t t = to_ms_since_boot(get_absolute_time());
    do {
        if (token == _spi_write(config, SPI_FILL_CHAR)) {
            return true;
        }
    } while (to_ms_since_boot(get_absolute_time()) < t + 300);    // Wait for 300 msec for start token
    debug_if(SD_DBG, "_wait_token: timeout\n");
    return false;
}

static bool _wait_ready(void *_config, uint64_t timeout) {
    blockdevice_sd_config_t *config = _config;
    uint8_t response;
    uint32_t t = to_ms_since_boot(get_absolute_time());
    do {
        spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, &response, sizeof(SPI_FILL_CHAR));
        if (response == 0xFF) {
            return true;
        }
    } while (to_ms_since_boot(get_absolute_time()) < t + timeout);
    return false;

}

static uint8_t _cmd_spi(void *_config, int cmd, uint32_t arg) {
    blockdevice_sd_config_t *config = _config;
    uint8_t response;
    uint8_t cmd_packet[PACKET_SIZE] = {0};

    // Prepare the command packet
    cmd_packet[0] = SPI_CMD(cmd);
    cmd_packet[1] = (arg >> 24);
    cmd_packet[2] = (arg >> 16);
    cmd_packet[3] = (arg >> 8);
    cmd_packet[4] = (arg >> 0);

    if (config->enable_crc) {
        uint8_t crc = _crc7(cmd_packet, 5);
        cmd_packet[5] = (crc << 1) | 0x01;
    } else {
        switch (cmd) {
        case CMD0_GO_IDLE_STATE:
            cmd_packet[5] = 0x95;
            break;
        case CMD8_SEND_IF_COND:
            cmd_packet[5] = 0x87;
            break;
        default:
            cmd_packet[5] = 0xFF;    // Make sure bit 0-End bit is high
            break;
        }
    }

    // send a command
    for (int i = 0; i < PACKET_SIZE; i++) {
        spi_write_read_blocking(config->spi_inst, (const uint8_t *)&cmd_packet[i], NULL, 1);
    }

    // The received byte immediataly following CMD12 is a stuff byte,
    // it should be discarded before receive the response of the CMD12.
    if (CMD12_STOP_TRANSMISSION == cmd) {
        spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, NULL, 1);
    }

    // Loop for response: Response is sent back within command response time (NCR), 0 to 8 bytes for SDC
    for (int i = 0; i < 0x10; i++) {
        spi_write_read_blocking(config->spi_inst, &SPI_FILL_CHAR, &response, 1);
        if (!(response & R1_RESPONSE_RECV)) {
            break;
        }
    }
    return response;
}

static int _cmd(void *_config, int cmd, uint32_t arg, bool is_acmd, uint32_t *resp) {
    blockdevice_sd_config_t *config = _config;
    int32_t status = BD_ERROR_OK;
    uint32_t response;

    _preclock_then_select(config);
    // No need to wait for card to be ready when sending the stop command
    if (CMD12_STOP_TRANSMISSION != cmd) {
        if (false == _wait_ready(config, SD_COMMAND_TIMEOUT)) {
            debug_if(SD_DBG, "Card not ready yet \n");
        }
    }

    // Re-try command
    for (int i = 0; i < 3; i++) {
        // Send CMD55 for APP command first
        if (is_acmd) {
            response = _cmd_spi(config, CMD55_APP_CMD, 0x0);
            // Wait for card to be ready after CMD55
            if (false == _wait_ready(config, SD_COMMAND_TIMEOUT)) {
                debug_if(SD_DBG, "Card not ready yet \n");
            }
        }

        // Send command over SPI interface
        response = _cmd_spi(config, cmd, arg);
        if (R1_NO_RESPONSE == response) {
            debug_if(SD_DBG, "No response CMD:%d \n", cmd);
            continue;
        }
        break;
    }

    // Pass the response to the command call if required
    if (NULL != resp) {
        *resp = response;
    }

    // Process the response R1  : Exit on CRC/Illegal command error/No response
    if (R1_NO_RESPONSE == response) {
        _postclock_then_deselect(config);
        debug_if(SD_DBG, "No response CMD:%d response: 0x%" PRIx32 "\n", cmd, response);
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;         // No device
    }
    if (response & R1_COM_CRC_ERROR) {
        _postclock_then_deselect(config);
        debug_if(SD_DBG, "CRC error CMD:%d response 0x%" PRIx32 "\n", cmd, response);
        return SD_BLOCK_DEVICE_ERROR_CRC;                // CRC error
    }
    if (response & R1_ILLEGAL_COMMAND) {
        _postclock_then_deselect(config);
        debug_if(SD_DBG, "Illegal command CMD:%d response 0x%" PRIx32 "\n", cmd, response);
        if (CMD8_SEND_IF_COND == cmd) {                  // Illegal command is for Ver1 or not SD Card
            config->card_type = CARD_UNKNOWN;
        }
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;      // Command not supported
    }

    debug_if(SD_DBG, "CMD:%d \t arg:0x%" PRIx32 " \t Response:0x%" PRIx32 "\n", cmd, arg, response);
    // Set status for other errors
    if ((response & R1_ERASE_RESET) || (response & R1_ERASE_SEQUENCE_ERROR)) {
        status = SD_BLOCK_DEVICE_ERROR_ERASE;            // Erase error
    } else if ((response & R1_ADDRESS_ERROR) || (response & R1_PARAMETER_ERROR)) {
        // Misaligned address / invalid address block length
        status = SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    // Get rest of the response part for other commands
    switch (cmd) {
        case CMD8_SEND_IF_COND:             // Response R7
            debug_if(SD_DBG, "V2-Version Card\n");
            config->card_type = SDCARD_V2; // fallthrough
        // Note: No break here, need to read rest of the response
        case CMD58_READ_OCR:                // Response R3
            response  = (_spi_write(config, SPI_FILL_CHAR) << 24);
            response |= (_spi_write(config, SPI_FILL_CHAR) << 16);
            response |= (_spi_write(config, SPI_FILL_CHAR) << 8);
            response |= _spi_write(config, SPI_FILL_CHAR);
            debug_if(SD_DBG, "R3/R7: 0x%" PRIx32 "\n", response);
            break;

        case CMD12_STOP_TRANSMISSION:       // Response R1b
        case CMD38_ERASE:
            _wait_ready(config, SD_COMMAND_TIMEOUT);
            break;

        case ACMD13_SD_STATUS:             // Response R2
            response = _spi_write(config, SPI_FILL_CHAR);
            debug_if(SD_DBG, "R2: 0x%" PRIx32 "\n", response);
            break;

        default:                            // Response R1
            break;
    }

    // Pass the updated response to the command
    if (NULL != resp) {
        *resp = response;
    }

    // Do not deselect card if read is in progress.
    if (((CMD9_SEND_CSD == cmd) || (ACMD22_SEND_NUM_WR_BLOCKS == cmd) ||
            (CMD24_WRITE_BLOCK == cmd) || (CMD25_WRITE_MULTIPLE_BLOCK == cmd) ||
            (CMD17_READ_SINGLE_BLOCK == cmd) || (CMD18_READ_MULTIPLE_BLOCK == cmd))
            && (BD_ERROR_OK == status)) {
        return BD_ERROR_OK;
    }
    // Deselect card
    _postclock_then_deselect(config);
    return status;
}

static int _cmd8(void *_config) {
    blockdevice_sd_config_t *config = _config;
    uint32_t arg = (CMD8_PATTERN << 0);         // [7:0]check pattern
    uint32_t response = 0;
    int32_t status = BD_ERROR_OK;

    arg |= (0x1 << 8);  // 2.7-3.6V             // [11:8]supply voltage(VHS)

    status = _cmd(config, CMD8_SEND_IF_COND, arg, false, &response);
    // Verify voltage and pattern for V2 version of card
    if ((BD_ERROR_OK == status) && (SDCARD_V2 == config->card_type)) {
        // If check pattern is not matched, CMD8 communication is not valid
        if ((response & 0xFFF) != arg) {
            debug_if(SD_DBG, "CMD8 Pattern mismatch 0x%" PRIx32 " : 0x%" PRIx32 "\n", arg, response);
            config->card_type = CARD_UNKNOWN;
            status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        }
    }
    return status;
}

static uint32_t _go_idle_state(void *_config) {
    blockdevice_sd_config_t *config = _config;
    uint32_t response;

    for (size_t i = 0; i < SD_CMD0_GO_IDLE_STATE_RETRIES; i++) {
        _cmd(config, CMD0_GO_IDLE_STATE, 0x0, 0, &response);
        if (R1_IDLE_STATE == response) {
            break;
        }
        sleep_ms(1);
    }
    return response;
}

static int init_card(void *_config) {
    blockdevice_sd_config_t *config = _config;
    int32_t status = BD_ERROR_OK;
    uint32_t response, arg;

    _spi_init(config);
    if (_go_idle_state(config) != R1_IDLE_STATE) {
        debug_if(SD_DBG, "No disk, or could not put SD card in to SPI idle state\n");
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;
    }

    // Send CMD8, if the card rejects the command then it's probably using the
    // legacy protocol, or is a MMC, or just flat-out broken
    status = _cmd8(config);
    if (BD_ERROR_OK != status && SD_BLOCK_DEVICE_ERROR_UNSUPPORTED != status) {
        return status;
    }

    if (config->enable_crc) {
        status = _cmd(config, CMD59_CRC_ON_OFF, config->enable_crc, 0, NULL);
    }

    // Read OCR - CMD58 Response contains OCR register
    if (BD_ERROR_OK != (status = _cmd(config, CMD58_READ_OCR, 0x0, 0x0, &response))) {
        return status;
    }

    // Check if card supports voltage range: 3.3V
    if (!(response & OCR_3_3V)) {
        config->card_type = CARD_UNKNOWN;
        status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        return status;
    }

    // HCS is set 1 for HC/XC capacity cards for ACMD41, if supported
    arg = 0x0;
    if (SDCARD_V2 == config->card_type) {
        arg |= OCR_HCS_CCS;
    }

    /* Idle state bit in the R1 response of ACMD41 is used by the card to inform the host
     * if initialization of ACMD41 is completed. "1" indicates that the card is still initializing.
     * "0" indicates completion of initialization. The host repeatedly issues ACMD41 until
     * this bit is set to "0".
     */
    absolute_time_t timeout = make_timeout_time_ms(SD_COMMAND_TIMEOUT);
    do {
        status = _cmd(config, ACMD41_SD_SEND_OP_COND, arg, 1, &response);
    } while ((response & R1_IDLE_STATE) && (0 < absolute_time_diff_us(get_absolute_time(), timeout)));

    // Initialization complete: ACMD41 successful
    if ((BD_ERROR_OK != status) || (0x00 != response)) {
        config->card_type = CARD_UNKNOWN;
        debug_if(SD_DBG, "Timeout waiting for card\n");
        return status;
    }

    if (SDCARD_V2 == config->card_type) {
        // Get the card capacity CCS: CMD58
        if (BD_ERROR_OK == (status = _cmd(config, CMD58_READ_OCR, 0x0, 0x0, &response))) {
            // High Capacity card
            if (response & OCR_HCS_CCS) {
                config->card_type = SDCARD_V2HC;
                debug_if(SD_DBG, "Card Initialized: High Capacity Card \n");
            } else {
                debug_if(SD_DBG, "Card Initialized: Standard Capacity Card: Version 2.x \n");
            }
        }
    } else {
        config->card_type = SDCARD_V1;
        debug_if(SD_DBG, "Card Initialized: Version 1.x Card\n");
    }

    if (!config->enable_crc) {
        // Disable CRC
        status = _cmd(config, CMD59_CRC_ON_OFF, config->enable_crc, 0x0, &response);
    } else {
        status = _cmd(config, CMD59_CRC_ON_OFF, 0, 0x0, &response);
    }
    return status;
}

static uint32_t ext_bits(unsigned char *data, int msb, int lsb)
{
    uint32_t bits = 0;
    uint32_t size = 1 + msb - lsb;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t position = lsb + i;
        uint32_t byte = 15 - (position >> 3);
        uint32_t bit = position & 0x7;
        uint32_t value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}

static int _read_bytes(void *_config, uint8_t *buffer, uint32_t length) {
    blockdevice_sd_config_t *config = _config;
    uint16_t crc;

    // read until start byte (0xFE)
    if (false == _wait_token(config, SPI_START_BLOCK)) {
        debug_if(SD_DBG, "Read timeout\n");
        _postclock_then_deselect(config);
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }

    // read data
    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = _spi_write(config, SPI_FILL_CHAR);
    }

    // Read the CRC16 checksum for the data block
    crc = (_spi_write(config, SPI_FILL_CHAR) << 8);
    crc |= _spi_write(config, SPI_FILL_CHAR);

    if (config->enable_crc) {
        uint32_t crc_result = _crc16(buffer, length);
        // Compute and verify checksum
        if (crc_result != crc) {
            debug_if(SD_DBG, "_read_bytes: Invalid CRC received 0x%" PRIx16 " result of computation 0x%" PRIx32 "\n",
                     crc, crc_result);
            _postclock_then_deselect(config);
            return SD_BLOCK_DEVICE_ERROR_CRC;
        }
    }
    _postclock_then_deselect(config);
    return 0;
}

static uint64_t _sd_sectors(void *_config) {
    blockdevice_sd_config_t *config = _config;
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr;
    uint32_t hc_c_size;
    uint64_t blocks = 0, capacity = 0;

    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (_cmd(config, CMD9_SEND_CSD, 0x0, 0, NULL) != 0x0) {
        debug_if(SD_DBG, "Didn't get a response from the disk\n");
        return 0;
    }
    uint8_t csd[16];
    if (_read_bytes(config, csd, 16) != 0) {
        debug_if(SD_DBG, "Couldn't read csd response from disk\n");
        return 0;
    }

    // csd_structure : csd[127:126]
    int csd_structure = ext_bits(csd, 127, 126);
    switch (csd_structure) {
        case 0:
            c_size = ext_bits(csd, 73, 62);              // c_size        : csd[73:62]
            c_size_mult = ext_bits(csd, 49, 47);         // c_size_mult   : csd[49:47]
            read_bl_len = ext_bits(csd, 83, 80);         // read_bl_len   : csd[83:80] - the *maximum* read block length
            block_len = 1 << read_bl_len;                // BLOCK_LEN = 2^READ_BL_LEN
            mult = 1 << (c_size_mult + 2);               // MULT = 2^C_SIZE_MULT+2 (C_SIZE_MULT < 8)
            blocknr = (c_size + 1) * mult;               // BLOCKNR = (C_SIZE+1) * MULT
            capacity = (uint64_t)blocknr * block_len;  // memory capacity = BLOCKNR * BLOCK_LEN
            blocks = capacity / config->block_size;
            debug_if(SD_DBG, "Standard Capacity: c_size: %" PRIu32 " \n", c_size);

            // ERASE_BLK_EN = 1: Erase in multiple of 512 bytes supported
            if (ext_bits(csd, 46, 46)) {
                config->erase_size = BLOCK_SIZE_HC;
            } else {
                // ERASE_BLK_EN = 1: Erase in multiple of SECTOR_SIZE supported
                config->erase_size = BLOCK_SIZE_HC * (ext_bits(csd, 45, 39) + 1);
            }
            break;

        case 1:
            hc_c_size = ext_bits(csd, 69, 48);            // device size : C_SIZE : [69:48]
            blocks = (hc_c_size + 1) << 10;               // block count = C_SIZE+1) * 1K byte (512B is block size)
            debug_if(SD_DBG, "SDHC/SDXC Card: hc_c_size: %" PRIu32 " \n", hc_c_size);
            // ERASE_BLK_EN is fixed to 1, which means host can erase one or multiple of 512 bytes.
            config->erase_size = BLOCK_SIZE_HC;
            break;

        default:
            debug_if(SD_DBG, "CSD struct unsupported\r\n");
            return 0;
    };
    return blocks;
}

static int _freq(void *_config) {
    blockdevice_sd_config_t *config = _config;

    // Max frequency supported is 25MHZ
    if (config->hz <= 25000000) {
        spi_set_baudrate(config->spi_inst, config->hz);
        return 0;
    } else {  // TODO: Switch function to be implemented for higher frequency
        config->hz = 25000000;
        spi_set_baudrate(config->spi_inst, config->hz);
        return -EINVAL;
    }
}

static int init(blockdevice_t *device) {
    blockdevice_sd_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    int err = init_card(config);
    config->is_initialized = (err == BD_ERROR_OK);
    if (!config->is_initialized) {
        debug_if(SD_DBG, "Fail to initialize card\n");
        mutex_exit(&config->_mutex);
        return err;
    }
    debug_if(SD_DBG, "init card = %d\n", config->is_initialized);
    config->total_sectors = _sd_sectors(config);
    // CMD9 failed
    if (0 == config->total_sectors) {
        mutex_exit(&config->_mutex);
        return BD_ERROR_DEVICE_ERROR;
    }

    // Set block length to 512 (CMD16)
    if (_cmd(config, CMD16_SET_BLOCKLEN, config->block_size, 0, NULL) != 0) {
        debug_if(SD_DBG, "Set %" PRIu32 "-byte block timed out\n", config->block_size);
        mutex_exit(&config->_mutex);
        return BD_ERROR_DEVICE_ERROR;
    }

    // Set SCK for data transfer
    err = _freq(config);
    if (err) {
        mutex_exit(&config->_mutex);
        return err;
    }

    device->is_initialized = true;
    mutex_exit(&config->_mutex);
    return BD_ERROR_OK;
}

static bool is_valid_read(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    return (addr % device->read_size == 0 &&
            size % device->read_size == 0 &&
            addr + size <= device->size(device));
}

static bool is_valid_program(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    return (addr % device->program_size == 0 &&
            size % device->program_size == 0 &&
            addr + size <= device->size(device));
}

static int deinit(blockdevice_t *device) {
    device->is_initialized = false;
    return 0;
}

static int sync(blockdevice_t *device) {
    (void)device;
    return 0;
}

static int _read(void *_config, uint8_t *buffer, uint32_t length) {
    blockdevice_sd_config_t *config = _config;

    uint16_t crc;

    // read until start byte (0xFE)
    if (false == _wait_token(config, SPI_START_BLOCK)) {
        debug_if(SD_DBG, "Read timeout\n");
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }

    // read data
    spi_read_blocking(config->spi_inst, 0xFF, buffer, length);

    // Read the CRC16 checksum for the data block
    crc = (_spi_write(config, SPI_FILL_CHAR) << 8);
    crc |= _spi_write(config, SPI_FILL_CHAR);

    if (config->enable_crc) {
        uint16_t crc_result = _crc16(buffer, length);
        if (crc_result != crc) {
            debug_if(SD_DBG, "_read_bytes: Invalid CRC received 0x%" PRIx16 " result of computation 0x%" PRIx16 "\n",
                     crc, crc_result);
            return SD_BLOCK_DEVICE_ERROR_CRC;
        }
    }

    return 0;
}

static int read(blockdevice_t *device, const void *_buffer, bd_size_t addr, bd_size_t size) {
    blockdevice_sd_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    if (!is_valid_read(device, addr, size)) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!config->is_initialized) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    uint8_t *buffer = (uint8_t *)_buffer;
    int status = BD_ERROR_OK;
    size_t block_count =  size / config->block_size;

    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == config->card_type) {
        addr = addr / config->block_size;
    }

    // Write command ro receive data
    if (block_count > 1) {
        status = _cmd(config, CMD18_READ_MULTIPLE_BLOCK, addr, 0, NULL);
    } else {
        status = _cmd(config, CMD17_READ_SINGLE_BLOCK, addr, 0, NULL);
    }
    if (BD_ERROR_OK != status) {
        mutex_exit(&config->_mutex);
        return status;
    }

    // receive the data : one block at a time
    while (block_count) {
        if (0 != _read(config, buffer, config->block_size)) {
            status = SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
            break;
        }
        buffer += config->block_size;
        --block_count;
    }
    _postclock_then_deselect(config);

    // Send CMD12(0x00000000) to stop the transmission for multi-block transfer
    if (size > config->block_size) {
        status = _cmd(config, CMD12_STOP_TRANSMISSION, 0x0, 0, NULL);
    }

    mutex_exit(&config->_mutex);
    return status;
}

static uint8_t _write(void *_config, const uint8_t *buffer, uint8_t token, uint32_t length) {
    blockdevice_sd_config_t *config = _config;

    uint32_t crc = (~0);
    uint8_t response = 0xFF;

    // indicate start of block
    _spi_write(config, token);

    // write the data
    spi_write_blocking(config->spi_inst, buffer, length);

    if (config->enable_crc) {
        crc = _crc16(buffer, length);
    }

    // write the checksum CRC-16
    _spi_write(config, crc >> 8);
    _spi_write(config, crc);


    // check the response token
    response = _spi_write(config, SPI_FILL_CHAR);

    // Wait for last block to be written
    if (false == _wait_ready(config, SD_COMMAND_TIMEOUT)) {
        debug_if(SD_DBG, "Card not ready yet \n");
    }

    return (response & SPI_DATA_RESPONSE_MASK);
}


static int program(blockdevice_t *device, const void *_buffer, bd_size_t addr, bd_size_t size) {
    blockdevice_sd_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    if (!is_valid_program(device, addr, size)) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!config->is_initialized) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_NO_INIT;
    }

    const uint8_t *buffer = (const uint8_t *)_buffer;
    int status = BD_ERROR_OK;
    uint8_t response;

    // Get block count
    size_t block_count = size / config->block_size;

    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == config->card_type) {
        addr = addr / config->block_size;
    }

    // Send command to perform write operation
    if (block_count == 1) {
        // Single block write command
        if (BD_ERROR_OK != (status = _cmd(config, CMD24_WRITE_BLOCK, addr, 0, NULL))) {
            mutex_exit(&config->_mutex);
            return status;
        }

        // Write data 
        response = _write(config, buffer, SPI_START_BLOCK, config->block_size);

        // Only CRC and general write error are communicated via response token
        if (response != SPI_DATA_ACCEPTED) {
            debug_if(SD_DBG, "Single Block Write failed: 0x%x \n", response);
            status = SD_BLOCK_DEVICE_ERROR_WRITE;
        }
    } else {
        // Pre-erase setting prior to multiple block write operation
        _cmd(config, ACMD23_SET_WR_BLK_ERASE_COUNT, block_count, 1, NULL);

        // Multiple block write command
        if (BD_ERROR_OK != (status = _cmd(config, CMD25_WRITE_MULTIPLE_BLOCK, addr, 0, NULL))) {
            mutex_exit(&config->_mutex);
            return status;
        }

        // Write the data: one block at a time
        do {
            response = _write(config, buffer, SPI_START_BLK_MUL_WRITE, config->block_size);
            if (response != SPI_DATA_ACCEPTED) {
                debug_if(SD_DBG, "Multiple Block Write failed: 0x%x \n", response);
                break;
            }
            buffer += config->block_size;
        } while (--block_count);     // Receive all blocks of data

        /* In a Multiple Block write operation, the stop transmission will be done by
         * sending 'Stop Tran' token instead of 'Start Block' token at the beginning
         * of the next block
         */
        _spi_write(config, SPI_STOP_TRAN);
    }

    _postclock_then_deselect(config);
    mutex_exit(&config->_mutex);
    return status;
}

static int erase(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    (void)device;
    (void)addr;
    (void)size;
    return 0;
}

static bool _is_valid_trim(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    blockdevice_sd_config_t *config = device->config;

    return (addr % config->erase_size == 0 &&
            size % config->erase_size == 0 &&
            addr + size <= device->size(device));
}

static int trim(blockdevice_t *device, bd_size_t addr, bd_size_t size) {
    blockdevice_sd_config_t *config = device->config;
    mutex_enter_blocking(&config->_mutex);

    if (!_is_valid_trim(device, addr, size)) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    if (!config->is_initialized) {
        mutex_exit(&config->_mutex);
        return SD_BLOCK_DEVICE_ERROR_NO_INIT;
    }
    int status = BD_ERROR_OK;

    size -= config->block_size;
    // SDSC Card (CCS=0) uses byte unit address
    // SDHC and SDXC Cards (CCS=1) use block unit address (512 Bytes unit)
    if (SDCARD_V2HC == config->card_type) {
        size = size / config->block_size;
        addr = addr / config->block_size;
    }

    // Start lba sent in start command
    if (BD_ERROR_OK != (status = _cmd(config, CMD32_ERASE_WR_BLK_START_ADDR, addr, 0, NULL))) {
        mutex_exit(&config->_mutex);
        return status;
    }

    // End lba = addr+size sent in end addr command
    if (BD_ERROR_OK != (status = _cmd(config, CMD33_ERASE_WR_BLK_END_ADDR, addr + size, 0, NULL))) {
        mutex_exit(&config->_mutex);
        return status;
    }
    status = _cmd(config, CMD38_ERASE, 0x0, 0, NULL);

    mutex_exit(&config->_mutex);
    return status;
}

static bd_size_t size(blockdevice_t *device) {
    blockdevice_sd_config_t *config = device->config;
    return config->block_size * config->total_sectors;
}

blockdevice_t *blockdevice_sd_create(spi_inst_t *spi_inst,
                                     uint8_t mosi,
                                     uint8_t miso,
                                     uint8_t sclk,
                                     uint8_t cs,
                                     uint32_t hz,
                                     bool enable_crc)
{
    if (spi_inst == spi0) {
        assert(mosi == 3 || mosi == 7 || mosi == 19 || mosi == 23);
        assert(miso == 0 || miso == 4 || miso == 16 || miso == 20);
        assert(sclk == 2 || sclk == 6 || sclk == 18 || sclk == 22);
        // assert(cs == 1 || cs == 5 || cs == 17 || cs == 21);
    } else if (spi_inst == spi1) {
        assert(mosi == 11 || mosi == 15 || mosi == 27);
        assert(miso == 8 || miso == 12 || mosi == 24 || mosi == 28);
        assert(sclk == 10 || sclk == 14 || sclk == 26);
        // assert(cs == 9 || cs == 13 || cs == 25 || cs == 29);
    } else {
        assert(spi_inst == spi0 || spi_inst == spi1);
    }

    blockdevice_t *device = calloc(1, sizeof(blockdevice_t));
    if (device == NULL) {
        fprintf(stderr, "blockdevice_sd_create: Out of memory\n");
        return NULL;
    }
    blockdevice_sd_config_t *config = calloc(1, sizeof(blockdevice_sd_config_t));
    if (config == NULL) {
        fprintf(stderr, "blockdevice_sd_create: Out of memory\n");
        free(device);
        return NULL;
    }

    device->init = init;
    device->deinit = deinit;
    device->sync = sync;
    device->read = read;
    device->erase = erase;
    device->program = program;
    device->trim = trim;
    device->size = size;
    device->read_size = block_size;
    device->erase_size = block_size;
    device->program_size = block_size;
    device->name = DEVICE_NAME;
    device->is_initialized = false;

    config->spi_inst = spi_inst;
    config->mosi = mosi;
    config->miso = miso;
    config->sclk = sclk;
    config->cs   = cs;
    config->hz   = hz;
    config->enable_crc = enable_crc;
    config->card_type = SDCARD_NONE;
    config->block_size = 512;
    mutex_init(&config->_mutex);
    device->config = config;

    device->init(device);
    return device;
}

void blockdevice_sd_free(blockdevice_t *device) {
    free(device->config);
    device->config = NULL;
    free(device);
    device = NULL;
}
