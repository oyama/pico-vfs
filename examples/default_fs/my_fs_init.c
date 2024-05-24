#include <hardware/clocks.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"


bool fs_init(void) {
    printf("create On-board flash block device\n");
    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - DEFAULT_FS_SIZE, 0);
    printf("create littlefs file system\n");
    filesystem_t *lfs = filesystem_littlefs_create(500, 16);
    printf("mount /\n");
    fs_mount("/", lfs, flash);

    printf("create SD block device\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              false);
    printf("create FAT file system\n");
    filesystem_t *fat = filesystem_fat_create();
    printf("mount /sd\n");
    fs_mount("/sd", fat, sd);

    return true;
}
