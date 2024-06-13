#include <assert.h>
#include <hardware/clocks.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/littlefs.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"


#define COLOR_GREEN(format)  ("\e[32m" format "\e[0m")

struct combination_map {
    blockdevice_t *device1;
    filesystem_t *filesystem1;
    blockdevice_t *device2;
    filesystem_t *filesystem2;
};

#define NUM_COMBINATION    6
static struct combination_map combination[NUM_COMBINATION];


static void init_filesystem_combination(void) {
    blockdevice_t *flash1 = blockdevice_flash_create(1 * 1024 * 1024, 512 * 1024);
    blockdevice_t *flash2 = blockdevice_flash_create(1 * 1024 * 1024 + 512 * 1024, 0);
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              24 * MHZ,
                                              true);
    filesystem_t *fat1 = filesystem_fat_create();
    filesystem_t *fat2 = filesystem_fat_create();
    filesystem_t *littlefs1 = filesystem_littlefs_create(500, 16);
    filesystem_t *littlefs2 = filesystem_littlefs_create(500, 16);

    combination[0] = (struct combination_map){
        .device1 = flash1, .filesystem1 = fat1, .device2 = flash2, .filesystem2 = fat2,
    };
    combination[1] = (struct combination_map){
        .device1 = flash1, .filesystem1 = fat1, .device2 = flash2, .filesystem2 = littlefs2,
    };
    combination[2] = (struct combination_map){
        .device1 = flash1, .filesystem1 = littlefs1, .device2 = flash2, .filesystem2 = littlefs2,
    };
    combination[3] = (struct combination_map){
        .device1 = flash1, .filesystem1 = fat1, .device2 = sd, .filesystem2 = fat2,
    };
    combination[4] = (struct combination_map){
        .device1 = flash1, .filesystem1 = fat1, .device2 = sd, .filesystem2 = littlefs2,
    };
    combination[5] = (struct combination_map){
        .device1 = flash1, .filesystem1 = littlefs1, .device2 = sd, .filesystem2 = littlefs2,
    };
}

#define TEST_FILE_SIZE  100 * 1024;

static void test_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vprintf(format, args);
    va_end(args);

    printf(" ");
    for (size_t i = 0; i < 50 - (size_t)n; i++)
        printf(".");
}

static void test_write(const char *path) {

    int fd = open(path, O_WRONLY|O_CREAT);
    assert(fd >= 0);

    uint8_t buffer[1024*64] = {0};
    size_t remind = TEST_FILE_SIZE;
    while (remind > 0) {
        size_t chunk = remind % sizeof(buffer) ? remind % sizeof(buffer) : sizeof(buffer);
        for (size_t i = 0; i < chunk; i++)
            buffer[i] = rand() & 0xFF;
        ssize_t write_size = write(fd, buffer, chunk);
        assert(write_size > 0);

        remind = remind - write_size;
    }
    int err = close(fd);
    assert(err == 0);
}

static void test_copy(const char *source, const char *dist) {
    int fd_src = open(source, O_RDONLY);
    assert(fd_src >= 0);
    int fd_dist = open(dist, O_WRONLY|O_CREAT);
    assert(fd_dist >= 0);
    assert(fd_src != fd_dist);

    uint8_t buffer[1024*64] = {0};
    while (true) {
        ssize_t read_size = read(fd_src, buffer, sizeof(buffer));
        if (read_size == 0)
            break;
        ssize_t write_size = write(fd_dist, buffer, read_size);
        assert(read_size == write_size);
    }

    int err = close(fd_src);
    assert(err == 0);
    err = close(fd_dist);
    assert(err == 0);
}

static void test_read(const char *path) {

    int fd = open(path, O_RDONLY);
    assert(fd >= 0);

    uint8_t buffer[1024*64] = {0};
    while (true) {
        ssize_t read_size = read(fd, buffer, sizeof(buffer));
        if (read_size == 0)
            break;
        for (size_t i = 0; i < (size_t)read_size; i++) {
            volatile uint8_t r = rand() & 0xFF;
            assert(buffer[i] == r);
        }
    }

    int err = close(fd);
    assert(err == 0);
}

void test_copy_between_different_filesystems(void) {
    printf("Copy between different file system:\n");

    init_filesystem_combination();

    for (size_t i = 0; i < NUM_COMBINATION; i++) {
        struct combination_map setting = combination[i];
        test_printf("from %s(%s) to %s(%s)",
                    setting.filesystem1->name, setting.device1->name,
                    setting.filesystem2->name, setting.device2->name);

        int err = fs_format(setting.filesystem1, setting.device1);
        if (err == -1 && errno == 5005) {
            printf("skip, device not connected\n");
            continue;
        }
        assert(err == 0);
        err = fs_format(setting.filesystem2, setting.device2);
        if (err == -1 && errno == 5005) {
            printf("skip, device not connected\n");
            continue;
        }
        assert(err == 0);

        err = fs_mount("/a", setting.filesystem1, setting.device1);
        assert(err == 0);
        err = fs_mount("/b", setting.filesystem2, setting.device2);
        assert(err == 0);

        srand(i);
        test_write("/a/source");
        test_copy("/a/source", "/b/dist");
        srand(i);
        test_read("/b/dist");
        printf(COLOR_GREEN("ok\n"));

        test_printf("from %s(%s) to %s(%s)",
                    setting.filesystem2->name, setting.device2->name,
                    setting.filesystem1->name, setting.device1->name),

        srand(i);
        test_write("/b/source");
        test_copy("/b/source", "/a/dist");
        srand(i);
        test_read("/a/dist");

        err = fs_unmount("/a");
        assert(err == 0);
        err = fs_unmount("/b");
        assert(err == 0);

        printf(COLOR_GREEN("ok\n"));
    }
}
