#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "blockdevice/flash.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define COLOR_GREEN(format)      ("\e[32m" format "\e[0m")
#define FLASH_START_AT           (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL         0
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
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static void cleanup(blockdevice_t *device) {
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static void test_api_format(filesystem_t *fs, blockdevice_t *device) {
    test_printf("fs_format");

    int err = fs_format(fs, device);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_mount(filesystem_t *fs, blockdevice_t *device) {
    test_printf("fs_mount");

    int err = fs_mount("/", fs, device);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_open_close() {
    test_printf("fs_open,fs_close");

    int fd = fs_open("/file", O_RDONLY);  // non-existing file
    assert(fd == -ENOENT);

    fd = fs_open("/file", O_WRONLY|O_CREAT);
    assert(fd == 0);

    int err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_write_read() {
    test_printf("fs_write,fs_read");

    int fd = fs_open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);

    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    int err = fs_close(fd);
    assert(err == 0);

    fd = fs_open("/file", O_RDONLY);
    assert(fd >= 0);
    char read_buffer[512] = {0};
    ssize_t read_length = fs_read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_seek() {
    test_printf("fs_seek");

    int fd = fs_open("/file", O_RDWR|O_CREAT);
    assert(fd >= 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs_seek(fd, 0, SEEK_SET);
    assert(offset == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = fs_read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(write_buffer, read_buffer) == 0);

    offset = fs_seek(fd, 9, SEEK_SET);
    assert(offset == 9);
    memset(read_buffer, 0, sizeof(read_buffer));
    read_length = fs_read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer) - 9);
    assert(strcmp("ABCDEF", read_buffer) == 0);

    int err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_tell() {
    test_printf("fs_tell");

    int fd = fs_open("/file", O_RDWR|O_CREAT);
    assert(fd >= 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs_seek(fd, 0, SEEK_SET);
    assert(offset == 0);

    off_t pos = fs_tell(fd);
    assert(pos == 0);

    offset = fs_seek(fd, 0, SEEK_END);
    assert((size_t)offset == strlen(write_buffer));

    pos = fs_tell(fd);
    assert((size_t)pos == strlen(write_buffer));

    int err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_truncate() {
    test_printf("fs_truncate");

    int fd = fs_open("/file", O_RDWR|O_CREAT);
    assert(fd >= 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs_seek(fd, 0, SEEK_SET);
    assert(offset == 0);

    int err = fs_truncate(fd, 9);
    assert(err == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = fs_read(fd, read_buffer, sizeof(read_buffer));
    assert(read_length == 9);
    assert(strcmp(read_buffer, "123456789") == 0);

    err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_open() {
    test_printf("fs_opendirn,fs_closedir");

    fs_dir_t *dir = fs_opendir("/dir");  // non-exists directory
    assert(dir == NULL);

    int err = fs_mkdir("/dir", 0777);
    assert(err == 0);

    dir = fs_opendir("/dir");
    assert(dir != NULL);

    err = fs_closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_read() {
    test_printf("fs_readdir");

    int err = fs_mkdir("/dir", 0777);
    assert(err == 0 || err == -EEXIST);

    // add regular file
    int fd = fs_open("/dir/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    err = fs_close(fd);
    assert(err == 0);

    fs_dir_t *dir = fs_opendir("/dir");
    assert(dir != NULL);

    struct dirent *ent = fs_readdir(dir);
    assert(ent != NULL);
    if (ent->d_type == DT_DIR) {
        // for not FAT file system
        assert(strcmp(ent->d_name, ".") == 0);

        ent = fs_readdir(dir);
        assert(ent != NULL);
        assert(ent->d_type == DT_DIR);
        assert(strcmp(ent->d_name, "..") == 0);

        ent = fs_readdir(dir);
        assert(ent != NULL);
    }
    assert(ent->d_type == DT_REG);
    assert(strcmp(ent->d_name, "file") == 0);

    ent = fs_readdir(dir);  // Reach the end of the directory
    assert(ent == NULL);

    err = fs_closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_remove() {
    test_printf("fs_unlink");

    int err = fs_unlink("/not-exists");
    assert(err == -ENOENT);

    int fd = fs_open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    err = fs_close(fd);
    assert(err == 0);

    err = fs_unlink("/file");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_rename() {
    test_printf("fs_rename");

    int err = fs_rename("/not-exists", "/renamed");
    assert(err == -ENOENT);

    int fd = fs_open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = fs_close(fd);
    assert(err == 0);

    err = fs_rename("/file", "/renamed");
    assert(err == 0);

    fd = fs_open("/renamed", O_RDONLY);
    assert(fd >= 0);
    char read_buffer[512] = {0};
    ssize_t read_length = fs_read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = fs_close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_stat() {
    test_printf("fs_stat");

    // regular file
    int fd = fs_open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs_write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    int err = fs_close(fd);
    assert(err == 0);

    struct stat finfo;
    err = fs_stat("/file", &finfo);
    assert(err == 0);
    assert((size_t)finfo.st_size == strlen(write_buffer));
    assert(finfo.st_mode & S_IFREG);

    // directory
    err = fs_mkdir("/dir", 0777);
    assert(err == 0 || err == -EEXIST);
    err = fs_stat("/dir", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFDIR);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_unmount(void) {
    test_printf("fs_mount");

    int err = fs_unmount("/");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

void test_vfs(void) {
    printf("Virtual file system layer(FAT):\n");

    blockdevice_t *flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    filesystem_t *fat = filesystem_fat_create();
    assert(fat != NULL);
    setup(flash);

    test_api_format(fat, flash);
    test_api_mount(fat, flash);
    test_api_file_open_close();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_dir_open();
    test_api_dir_read();
    test_api_remove();
    test_api_rename();
    test_api_stat();
    test_api_unmount();

    cleanup(flash);
    blockdevice_flash_free(flash);
    filesystem_fat_free(fat);

    printf("Virtual file system layer(littlefs):\n");

    flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    filesystem_t *lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                                   LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(flash);

    test_api_format(lfs, flash);
    test_api_mount(lfs, flash);
    test_api_file_open_close();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_dir_open();
    test_api_dir_read();
    test_api_remove();
    test_api_rename();
    test_api_stat();
    test_api_unmount();

    cleanup(flash);
    blockdevice_flash_free(flash);
    filesystem_littlefs_free(lfs);
}
