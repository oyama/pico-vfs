#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "blockdevice/flash.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"

static blockdevice_t *test_device;
static filesystem_t *test_fs;


static void setup(void) {
    test_device = blockdevice_flash_create(1 * 1024 * 1024, 0);
    test_fs = filesystem_fat_create();

    int err = fs_format(test_fs, test_device);
    assert(err == 0);
    err = fs_mount("/test", test_fs, test_device);
    assert(err == 0);
}

static void cleanup(void) {
    int err = fs_unmount("/test");
    assert(err == 0);

    filesystem_fat_free(test_fs);
    blockdevice_flash_free(test_device);
}

static void test_format(void) {
    blockdevice_t *device = blockdevice_flash_create(1 * 1024 * 1024, 0);
    filesystem_t *fs = filesystem_fat_create();

    int err = fs_format(fs, device);
    assert(err == 0);

    filesystem_fat_free(fs);
    blockdevice_flash_free(device);
}

static void test_mount(void) {
    blockdevice_t *device = blockdevice_flash_create(1 * 1024 * 1024, 0);
    filesystem_t *fs = filesystem_fat_create();

    int err = fs_format(fs, device);
    assert(err == 0);
    err = fs_mount("/test", fs, device);
    assert(err == 0);
    err = fs_unmount("/test");
    assert(err == 0);

    filesystem_fat_free(fs);
    blockdevice_flash_free(device);
}

static void test_file_create_read_update_delete(void) {
    setup();

    // Create
    char write_buff[512] = "123456789";
    int fd = fs_open("/test/filename", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    ssize_t write_size = fs_write(fd, write_buff, strlen(write_buff));
    assert(write_size == strlen(write_buff));
    int err = fs_close(fd);
    assert(err == 0);

    // Read
    fd = fs_open("/test/filename", O_RDONLY);
    assert(fd >= 0);
    char read_buff[512] = {0};
    int read_size = fs_read(fd, read_buff, sizeof(read_buff));
    assert(read_size == strlen(write_buff));
    assert(strcmp(write_buff, read_buff) == 0);
    err = fs_close(fd);
    assert(err == 0);

    // Update
    char update_buff[512] = "ABCDEFG";
    fd = fs_open("/test/filename", O_WRONLY);
    assert(fd >= 0);
    ssize_t update_size = fs_write(fd, update_buff, strlen(update_buff));
    assert(update_size == strlen(update_buff));
    assert(update_size == fs_tell(fd));
    err = fs_truncate(fd, fs_tell(fd));
    assert(err == 0);
    err = fs_close(fd);
    assert(err == 0);

    // Delete
    err = fs_unlink("/test/filename");
    assert(err == 0);

    cleanup();
}

void test_blockdevice_flash_filesystem_fat(void) {

    printf("Flash-FAT filesystem..............");

    test_format();
    test_mount();
    test_file_create_read_update_delete();

    printf("ok\n");
}
