#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <hardware/clocks.h>
#include "blockdevice/flash.h"
#include "blockdevice/heap.h"
#include "blockdevice/loopback.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

#define COLOR_GREEN(format)      ("\e[32m" format "\e[0m")
#define FLASH_START_AT           (0.5 * 1024 * 1024)
#define FLASH_LENGTH_ALL         0
#define LITTLEFS_BLOCK_CYCLE     500
#define LITTLEFS_LOOKAHEAD_SIZE  16
#define MIN_FILENO               3
#define BLOCKDEVICE_HEAP_SIZE    (64 * 1024)

#define PICO_SPI1_TX_PIN   15
#define PICO_SPI1_RX_PIN   12
#define PICO_SPI1_SCK_PIN  14
#define PICO_SPI1_CSN_PIN  13

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
    // Deinit is performed when unmounting, so re-init is required.
    device->init(device);
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

static void test_api_format_error(filesystem_t *fs, blockdevice_t *device) {
    test_printf("fs_format error");

    int err = fs_format(fs, device);
    assert(err == -1);
    assert(errno == 5005);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_mount_error(filesystem_t *fs, blockdevice_t *device) {
    test_printf("fs_mount error");

    int err = fs_mount("/", fs, device);
    assert(err == -1);
    assert(errno == 5005);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_open_close() {
    test_printf("open,close");

    int fd = open("/file", O_RDONLY);  // non-existing file
    assert(fd == -1);
    assert(errno == ENOENT);

    fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= MIN_FILENO);

    int err = close(fd);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_open_many() {
    test_printf("open many files");

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd >= MIN_FILENO);

    int err = close(fd);
    assert(err == 0);

    int fd1 = open("/file", O_WRONLY|O_CREAT);
    assert(fd1 >= MIN_FILENO);
    int fd2 = open("/file2", O_WRONLY|O_CREAT);
    assert(fd2 == fd1 + 1);
    int fd3 = open("/file3", O_WRONLY|O_CREAT);
    assert(fd3 == fd1 + 2);
    int fd4 = open("/file4", O_WRONLY|O_CREAT);
    assert(fd4 == fd1 + 3);
    int fd5 = open("/file5", O_WRONLY|O_CREAT);
    assert(fd5 == fd1 + 4);

    err = close(fd5);
    assert(err == 0);
    err = close(fd4);
    assert(err == 0);
    err = close(fd3);
    assert(err == 0);
    err = close(fd2);
    assert(err == 0);
    err = close(fd1);
    assert(err == 0);

    int fd6 = open("/file6", O_WRONLY|O_CREAT);
    assert(fd6 == fd1);
    err = close(fd6);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}


static void test_api_file_write_read() {
    test_printf("write,read");

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd != -1);

    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    int err = close(fd);
    assert(err == 0);

    fd = open("/file", O_RDONLY);
    assert(fd != -1);
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
    assert(fd != -1);

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
    assert(fd != -1);

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

static void test_api_mkdir() {
    test_printf("mkdir,rmdir");

    int err = mkdir("/dir-create", 0777);
    assert(err == 0);

    struct stat finfo;
    err = stat("/dir-create", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFDIR);

    err = rmdir("/dir-create");
    assert(err == 0);

    err = stat("/dir-create", &finfo);
    assert(err == -1 && errno == ENOENT);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_open() {
    test_printf("opendir,closedir");

    DIR *dir = opendir("/dir-non-exists");  // non-exists directory
    assert(dir == NULL);
    assert((errno == ENOTDIR) || (errno == ENOENT));  // NOTE: FAT returns ENOTDIR, littlefs returns ENOENT

    int err = mkdir("/dir", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));

    dir = opendir("/dir");
    assert(dir != NULL);

    err = closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_open_many() {
    test_printf("opendir many dir");

    int err = mkdir("/dir1", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));
    err = mkdir("/dir2", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));
    err = mkdir("/dir3", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));
    err = mkdir("/dir4", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));
    err = mkdir("/dir5", 0777);
    assert((err == 0) || (err -1 && errno == EEXIST));

    DIR *dir1 = opendir("/dir1");
    assert(dir1 != NULL);
    DIR *dir2 = opendir("/dir2");
    assert(dir2 != NULL);
    DIR *dir3 = opendir("/dir3");
    assert(dir3 != NULL);
    DIR *dir4 = opendir("/dir4");
    assert(dir4 != NULL);
    DIR *dir5 = opendir("/dir5");
    assert(dir5 != NULL);

    err = closedir(dir5);
    assert(err == 0);
    err = closedir(dir4);
    assert(err == 0);
    err = closedir(dir3);
    assert(err == 0);
    err = closedir(dir2);
    assert(err == 0);
    err = closedir(dir1);
    assert(err == 0);
    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_read() {
    test_printf("readdir");

    int err = mkdir("/dir", 0777);
    assert(err == 0 || (err == -1 && errno == EEXIST));

    // add regular file
    int fd = open("/dir/file", O_WRONLY|O_CREAT);
    assert(fd != -1);
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
    assert(errno == 0);

    err = closedir(dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_remove() {
    test_printf("unlink");

    int err = unlink("/not-exists");
    assert(err == -1);
    assert(errno == ENOENT);

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd != -1);
    err = close(fd);
    assert(err == 0);

    err = unlink("/file");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_rename() {
    test_printf("rename");

    int err = rename("/not-exists", "/renamed");
    assert(err == -1);
    assert(errno == ENOENT);

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd != -1);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = close(fd);
    assert(err == 0);

    err = rename("/file", "/renamed");
    assert(err == 0);

    fd = open("/renamed", O_RDONLY);
    assert(fd != -1);
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
    assert(fd != -1);
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
    assert(err == 0 || (err == -1 && errno == EEXIST));
    err = stat("/dir", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFDIR);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_reformat(void) {
    test_printf("fs_reformat");

    int fd = open("/file", O_WRONLY|O_CREAT);
    assert(fd != -1);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = write(fd, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));
    int err = close(fd);
    assert(err == 0);

    err = fs_reformat("/");
    assert(err == 0);

    struct stat finfo;
    err = stat("/file", &finfo);
    assert(err == -1);
    assert(errno == ENOENT);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_unmount(void) {
    test_printf("fs_unmount");

    int err = fs_unmount("/");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_mount_unmount_repeat(filesystem_t *fs, blockdevice_t *device) {
    test_printf("fs_mount,fs_unmount repeat");

    for (size_t i = 0; i < 20; i++) {
        int err = fs_mount("/", fs, device);
        assert(err == 0);
        err = fs_unmount("/");
        assert(err == 0);
    }

    printf(COLOR_GREEN("ok\n"));
}

static void test_loopback_file(void) {
    test_printf("loopback image file");

    struct stat finfo;
    int err = stat("/flash/loopback.dmg", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFREG);
    assert((size_t)finfo.st_size > 0);

    printf(COLOR_GREEN("ok\n"));
}

void test_vfs(void) {
    printf("VFS FAT:\n");
    blockdevice_t *flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    filesystem_t *fat = filesystem_fat_create();
    assert(fat != NULL);
    setup(flash);

    test_api_format(fat, flash);
    test_api_mount(fat, flash);
    test_api_file_open_close();
    test_api_file_open_many();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_stat();
    test_api_remove();
    test_api_rename();
    test_api_mkdir();
    test_api_dir_open();
    test_api_dir_open_many();
    test_api_dir_read();
    test_api_reformat();
    test_api_unmount();
    test_api_mount_unmount_repeat(fat, flash);

    cleanup(flash);
    blockdevice_flash_free(flash);
    filesystem_fat_free(fat);

    printf("VFS littlefs:\n");
    flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    filesystem_t *lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                                   LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(flash);

    test_api_format(lfs, flash);
    test_api_mount(lfs, flash);
    test_api_file_open_close();
    test_api_file_open_many();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_stat();
    test_api_remove();
    test_api_rename();
    test_api_mkdir();
    test_api_dir_open();
    test_api_dir_open_many();
    test_api_dir_read();
    test_api_reformat();
    test_api_unmount();
    test_api_mount_unmount_repeat(lfs, flash);

    cleanup(flash);
    blockdevice_flash_free(flash);
    filesystem_littlefs_free(lfs);

    printf("VFS littlefs on the Heap:\n");
    blockdevice_t *heap = blockdevice_heap_create(BLOCKDEVICE_HEAP_SIZE);
    assert(heap != NULL);
    lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                     LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(heap);

    test_api_format(lfs, heap);
    test_api_mount(lfs, heap);
    test_api_file_open_close();
    test_api_file_open_many();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_stat();
    test_api_remove();
    test_api_rename();
    test_api_mkdir();
    test_api_dir_open();
    test_api_dir_open_many();
    test_api_dir_read();
    test_api_reformat();
    test_api_unmount();
    test_api_mount_unmount_repeat(lfs, heap);

    cleanup(heap);
    filesystem_littlefs_free(lfs);
    blockdevice_heap_free(heap);


    printf("VFS loopback FAT on littlefs:\n");
    flash = blockdevice_flash_create(FLASH_START_AT, FLASH_LENGTH_ALL);
    assert(flash != NULL);
    lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                     LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(flash);
    int err = fs_format(lfs, flash);
    assert(err == 0);
    err = fs_mount("/flash", lfs, flash);
    assert(err == 0);

    blockdevice_t *loopback = blockdevice_loopback_create("/flash/loopback.dmg", 64 * 1024, 512);
    fat = filesystem_fat_create();
    assert(loopback != NULL);
    assert(fat != NULL);

    test_api_format(fat, loopback);
    test_api_mount(fat, loopback);
    test_api_file_open_close();
    test_api_file_open_many();
    test_api_file_write_read();
    test_api_file_seek();
    test_api_file_tell();
    test_api_file_truncate();
    test_api_stat();
    test_api_remove();
    test_api_rename();
    test_api_mkdir();
    test_api_dir_open();
    test_api_dir_open_many();
    test_api_dir_read();
    test_api_reformat();
    test_api_mount_unmount_repeat(fat, loopback);

    filesystem_fat_free(fat);
    blockdevice_loopback_free(loopback);

    test_loopback_file();

    fs_unmount("/flash");
    cleanup(flash);
    blockdevice_flash_free(flash);
    filesystem_littlefs_free(lfs);

    printf("VFS not connected SD card error handling:\n");
    blockdevice_t *sd = blockdevice_sd_create(spi1,  // SPI1 without connection
                                              PICO_SPI1_TX_PIN,
                                              PICO_SPI1_RX_PIN,
                                              PICO_SPI1_SCK_PIN,
                                              PICO_SPI1_CSN_PIN,
                                              10 * MHZ,
                                              false);
    assert(sd != NULL);
    fat = filesystem_fat_create();
    assert(fat != NULL);
    setup(sd);

    test_api_format_error(fat, sd);
    test_api_mount_error(fat, sd);

    filesystem_fat_free(fat);
    blockdevice_sd_free(sd);
}
