#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
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
    test_printf("open,close");

    int fd = open("/file", O_RDONLY);  // non-existing file
    assert(fd == -ENOENT);

    fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd == 0);

    int err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_write_read() {
    test_printf("write,read");

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);

    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    int err = close(fd);
    assert(err == 0);

    fd = open("/file", O_RDONLY);
    assert(fd >= 0);
    char read_buffer[512] = {0};
    ssize_t read_length = read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_seek() {
    test_printf("lseek");

    int fd = open("/file", O_RDWR|O_CREAT);
    assert(fd >= 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = lseek(fd, 0, SEEK_SET);
    assert(offset == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(write_buffer, read_buffer) == 0);

    offset = lseek(fd, 9, SEEK_SET);
    assert(offset == 9);
    memset(read_buffer, 0, sizeof(read_buffer));
    read_length = read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer) - 9);
    assert(strcmp("ABCDEF", read_buffer) == 0);

    int err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_tell() {
    test_printf("ftell");

    FILE *fp = fopen("/file", "w+");
    assert(fp != NULL);

    char write_data[] = "123456789ABCDEF";
    size_t write_length = fwrite(write_data, 1, strlen(write_data), fp);
    assert(write_length == strlen(write_data));

    int err = fflush(fp);
    assert(err == 0);

    long pos = ftell(fp);
    assert((size_t)pos == strlen(write_data));

    err = fseek(fp, 0, SEEK_SET);
    assert(err == 0);

    pos = ftell(fp);
    assert(pos == 0);

    err = fseek(fp, 0, SEEK_END);
    assert(err == 0);
    pos = ftell(fp);
    assert((size_t)pos == strlen(write_data));

    err = fclose(fp);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_truncate() {
    test_printf("ftruncate");

    int fd = open("/file", O_RDWR|O_CREAT);
    assert(fd >= 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = lseek(fd, 0, SEEK_SET);
    assert(offset == 0);

    int err = ftruncate(fd, 9);
    assert(err == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = read(fd, read_buffer, sizeof(read_buffer));
    assert(read_length == 9);
    assert(strcmp(read_buffer, "123456789") == 0);

    err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_open() {
    test_printf("opendir,closedir");

    DIR *dir = opendir("/dir");  // non-exists directory
    assert(dir == NULL);

    int err = mkdir("/dir", 0777);
    assert(err == 0);

    dir = opendir("/dir");
    assert(dir != NULL);

    err = closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_read() {
    test_printf("readdir");

    int err = mkdir("/dir", 0777);
    assert(err == 0 || err == -EEXIST);

    // add regular file
    int fd = open("/dir/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    err = close(fd);
    assert(err == 0);

    DIR *dir = opendir("/dir");
    assert(dir != NULL);

    struct dirent *ent = readdir(dir);
    assert(ent != NULL);
    if (ent->d_type == DT_DIR) {
        // for not FAT file system
        assert(strcmp(ent->d_name, ".") == 0);

        ent = readdir(dir);
        assert(ent != NULL);
        assert(ent->d_type == DT_DIR);
        assert(strcmp(ent->d_name, "..") == 0);

        ent = readdir(dir);
        assert(ent != NULL);
    }
    assert(ent->d_type == DT_REG);
    assert(strcmp(ent->d_name, "file") == 0);

    ent = readdir(dir);  // Reach the end of the directory
    assert(ent == NULL);

    err = closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_remove() {
    test_printf("unlink");

    int err = unlink("/not-exists");
    assert(err == -ENOENT);

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    err = close(fd);
    assert(err == 0);

    err = unlink("/file");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_rename() {
    test_printf("rename");

    int err = rename("/not-exists", "/renamed");
    assert(err == -ENOENT);

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = close(fd);
    assert(err == 0);

    err = rename("/file", "/renamed");
    assert(err == 0);

    fd = open("/renamed", O_RDONLY);
    assert(fd >= 0);
    char read_buffer[512] = {0};
    ssize_t read_length = read(fd, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_stat() {
    test_printf("lstat");

    // regular file
    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    int err = close(fd);
    assert(err == 0);

    struct stat finfo;
    err = stat("/file", &finfo);
    assert(err == 0);
    assert((size_t)finfo.st_size == strlen(write_buffer));
    assert(finfo.st_mode & S_IFREG);

    // directory
    err = mkdir("/dir", 0777);
    assert(err == 0 || err == -EEXIST);
    err = stat("/dir", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFDIR);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_unmount(void) {
    test_printf("fs_unmount");

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
