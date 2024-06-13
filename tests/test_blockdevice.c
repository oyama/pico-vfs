#include <assert.h>
#include <hardware/clocks.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"
#include "blockdevice/heap.h"
#include "blockdevice/loopback.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"

#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")
#define FLASH_START_AT       (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL     0
#define HEAP_STORAGE_SIZE    (512 * 128)   // 64KB
#define LOOPBACK_STORAGE_SIZE  1024
#define LOOPBACK_BLOCK_SIZE    512

#include <ctype.h>
static void print_hex(const char *label, const void *buffer, size_t length) {
    const uint8_t *buf = buffer;
    printf("%s:\n", label);
    size_t offset = 0;
    for (size_t i = 0; i < length; ++i) {
        if (i % 16 == 0)
            printf("0x%04u%s", offset, (i % 512) == 0 ? ">" : " ");
        if (isalnum(buf[i])) {
            printf("'%c' ", buf[i]);
        } else {
            printf("0x%02x", buf[i]);
        }
        if (i % 16 == 15) {
            printf("\n");
            offset += 16;
        } else {
            printf(", ");
        }
    }
}

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
    (void)device;
}

static void cleanup(blockdevice_t *device) {
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static bool is_sd_card_connected(blockdevice_t *device) {
    int err = device->deinit(device);
    assert(err == BD_ERROR_OK);

    err = device->init(device);
    if (err == BD_ERROR_OK)
        return true;
    else if (err == -5005)
        return false;
    else
        assert(err == BD_ERROR_OK);
    return false;
}

static void test_api_init(blockdevice_t *device) {
    test_printf("init");

    // The init is done when the object is created.
    int err = device->deinit(device);
    assert(err == BD_ERROR_OK);

    // Init for later testing
    err = device->init(device);
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

    printf("Block device SPI SD card:\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * MHZ,
                                              false);
    assert(sd != NULL);
    if (is_sd_card_connected(sd)) {
        setup(sd);

        test_api_init(sd);
        test_api_erase_program_read(sd);
        test_api_trim(sd);
        test_api_sync(sd);
        test_api_size(sd);
        test_api_attribute(sd);

        cleanup(sd);
    } else {
        test_printf("init");
        printf("skip, device not connected\n");
    }
    blockdevice_sd_free(sd);

    printf("Block device Heap memory:\n");
    blockdevice_t *heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    assert(heap != NULL);
    setup(heap);

    test_api_init(heap);
    test_api_erase_program_read(heap);
    test_api_trim(heap);
    test_api_sync(heap);
    test_api_size(heap);
    test_api_attribute(heap);

    cleanup(heap);
    blockdevice_heap_free(heap);

    printf("Block device Loopback:\n");
    heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    assert(heap != NULL);
    setup(heap);
    filesystem_t *fat = filesystem_fat_create();
    assert(fat != NULL);
    fs_format(fat, heap);
    fs_mount("/", fat, heap);
    blockdevice_t *loopback = blockdevice_loopback_create("/loopback",
                                                          LOOPBACK_STORAGE_SIZE,
                                                          LOOPBACK_BLOCK_SIZE);
    assert(loopback != NULL);

    test_api_init(loopback);
    test_api_erase_program_read(loopback);
    test_api_trim(loopback);
    test_api_sync(loopback);
    test_api_size(loopback);
    test_api_attribute(loopback);

    cleanup(loopback);
    blockdevice_loopback_free(loopback);
    fs_unmount("/");
    filesystem_fat_free(fat);
    cleanup(heap);
    blockdevice_heap_free(heap);
}
