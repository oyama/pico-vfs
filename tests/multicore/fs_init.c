#include <hardware/clocks.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define FLASH_LENGTH_ALL    0

bool fs_init(void) {
    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE,
                                                    FLASH_LENGTH_ALL);
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              true);
    filesystem_t *fat = filesystem_fat_create();
    filesystem_t *littlefs = filesystem_littlefs_create(500, 16);

    int err = fs_format(littlefs, flash);
    if (err == -1)
        return false;
    err = fs_mount("/flash", littlefs, flash);
    if (err == -1)
        return false;

    fs_format(fat, sd);
    if (err == -1)
        return false;
    err = fs_mount("/sd", fat, sd);
    if (err == -1)
        return false;
    return true;
}
