#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/heap.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"

#define COLOR_GREEN(format)      ("\e[32m" format "\e[0m")
#define HEAP_STORAGE_SIZE        (128 * 1024)
#define LITTLEFS_BLOCK_CYCLE     500
#define LITTLEFS_LOOKAHEAD_SIZE  16

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
    device->init(device);
}

static uint32_t xor_rand(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static uint32_t xor_rand_32bit(uint32_t *seed) {
    return xor_rand(seed);
}

static void test_api_write(filesystem_t *fs) {
    test_printf("file_write");

    uint8_t buffer[512];
    for (size_t i = 0; i < 10000; i++) {
        fs_file_t file;
        int err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
        assert(err == 0);

        uint32_t counter = 0;
        xor_rand(&counter);
        size_t remind = HEAP_STORAGE_SIZE * 0.4;
        while (remind > 0) {
            size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
            uint32_t *b = (uint32_t *)buffer;
            for (size_t j = 0; j < (chunk / sizeof(uint32_t)); j++)
                b[j] = xor_rand_32bit(&counter);
            ssize_t write_length = fs->file_write(fs, &file, buffer, chunk);
            assert((size_t)write_length == chunk);

            remind = remind - chunk;
        }
        err = fs->file_close(fs, &file);
        assert(err == 0);
    }

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_read(filesystem_t *fs) {
    test_printf("file_read");

    uint8_t buffer[512];
    for (size_t i = 0; i < 10000; i++) {
        fs_file_t file;
        int err = fs->file_open(fs, &file, "/file", O_RDONLY);
        assert(err == 0);

        uint32_t counter = 0;
        xor_rand(&counter);
        size_t remind = HEAP_STORAGE_SIZE * 0.4;
        while (remind > 0) {
            size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
            ssize_t read_length = fs->file_read(fs, &file, buffer, chunk);
            assert((size_t)read_length == chunk);

            uint32_t *b = (uint32_t *)buffer;
            for (size_t j = 0; j < chunk / sizeof(uint32_t); j++) {
                volatile uint32_t v = xor_rand_32bit(&counter);
                assert(b[j] == v);
            }
            remind = remind - read_length;
        }
        err = fs->file_close(fs, &file);
        assert(err == 0);
    }

    printf(COLOR_GREEN("ok\n"));
}

void test_benchmark(void) {
    printf("FAT write/read:\n");

    blockdevice_t *heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    assert(heap != NULL);
    filesystem_t *fat = filesystem_fat_create();
    assert(fat != NULL);
    setup(heap);
    int err = fat->format(fat, heap);
    assert(err == 0);
    err = fat->mount(fat, heap, false);
    assert(err == 0);

    test_api_write(fat);
    test_api_read(fat);

    err = fat->unmount(fat);
    assert(err == 0);
    cleanup(heap);
    filesystem_fat_free(fat);
    blockdevice_heap_free(heap);


    printf("littlefs write/read:\n");
    heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    assert(heap != NULL);
    filesystem_t *lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                                   LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(heap);
    err = lfs->format(lfs, heap);
    assert(err == 0);
    err = lfs->mount(lfs, heap, false);
    assert(err == 0);


    test_api_write(lfs);
    test_api_read(lfs);

    err = lfs->unmount(lfs);
    assert(err == 0);
    cleanup(heap);
    filesystem_littlefs_free(lfs);
    blockdevice_heap_free(heap);
}
