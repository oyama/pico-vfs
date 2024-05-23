#include <assert.h>
#include <hardware/clocks.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")
#define FLASH_START_AT       (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL     0


static void test_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vprintf(format, args);
    va_end(args);

    printf(" ");
    for (size_t i = 0; i < 50 - (size_t)n; i++)
        printf(".");
}

static void setup(blockdevice_t *device) {
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static void cleanup(blockdevice_t *device) {
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static void test_api_init(blockdevice_t *device) {
    test_printf("init");

    int err = device->init(device);
    assert(err == BD_ERROR_OK);

    err = device->deinit(device);
    assert(err == BD_ERROR_OK);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_sync(blockdevice_t *device) {
    test_printf("sync");

    int err = device->sync(device);
    assert(err == BD_ERROR_OK);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_erase_program_read(blockdevice_t *device) {
    test_printf("erase,program,read");

    size_t addr = 0;
    size_t length = device->erase_size;

    // erase test block
    int err = device->erase(device, addr, length);
    assert(err == BD_ERROR_OK);

    // program by random data
    uint8_t *program_buffer = calloc(1, length);
    srand(length);
    for (size_t i = 0; i < length; i++)
        program_buffer[i] = rand() & 0xFF;
    err = device->program(device, program_buffer, addr, length);
    assert(err == BD_ERROR_OK);

    // read test block
    uint8_t *read_buffer = calloc(1, length);
    err = device->read(device, read_buffer, addr, length);
    assert(err == BD_ERROR_OK);
    assert(memcmp(program_buffer, read_buffer, length) == 0);

    free(read_buffer);
    free(program_buffer);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_trim(blockdevice_t *device) {
    test_printf("trim");

    int err = device->trim(device, 0, 0);
    assert(err == BD_ERROR_OK);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_size(blockdevice_t *device) {
    test_printf("size");

    size_t length = device->size(device);
    assert(length > 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_attribute(blockdevice_t *device) {
    test_printf("attribute");

    assert(device->read_size > 0);
    assert(device->erase_size > 0);
    assert(device->program_size > 0);
    assert(strlen(device->name) > 0);

    assert(device->config != NULL);

    printf(COLOR_GREEN("ok\n"));
}

void test_blockdevice(void) {
    printf("Block device Onboard-Flash:\n");
    blockdevice_t *flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    setup(flash);

    test_api_init(flash);
    test_api_erase_program_read(flash);
    test_api_trim(flash);
    test_api_sync(flash);
    test_api_size(flash);
    test_api_attribute(flash);

    cleanup(flash);
    blockdevice_flash_free(flash);

#if !defined(WITHOUT_BLOCKDEVICE_SD)
    printf("Block device SPI SD card:\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * MHZ,
                                              false);
    assert(sd != NULL);
    setup(sd);

    test_api_init(sd);
    test_api_erase_program_read(sd);
    test_api_trim(sd);
    test_api_sync(sd);
    test_api_size(sd);
    test_api_attribute(sd);

    cleanup(sd);
    blockdevice_sd_free(sd);
#endif
}
