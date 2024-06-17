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
    size_t length = device->size(device);
    // Deinit is performed when unmounting, so re-init is required.
    device->init(device);
    device->erase(device, 0, length);
}

static void test_api_format(filesystem_t *fs, blockdevice_t *device) {
    test_printf("format");

    int err = fs->format(fs, device);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_mount(filesystem_t *fs, blockdevice_t *device) {
    test_printf("mount");

    int err = fs->mount(fs, device, false);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_unmount(filesystem_t *fs) {
    test_printf("unmount");

    int err = fs->unmount(fs);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_open_close(filesystem_t *fs) {
    test_printf("file_open,file_close");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_RDONLY);  // non-existing file
    assert(err == -ENOENT);

    err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
    assert(err == 0);

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_write_read(filesystem_t *fs) {
    test_printf("file_write,file_read");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
    assert(err == 0);

    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = fs->file_close(fs, &file);
    assert(err == 0);

    err = fs->file_open(fs, &file, "/file", O_RDONLY);
    assert(err == 0);
    char read_buffer[512] = {0};
    ssize_t read_length = fs->file_read(fs, &file, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_seek(filesystem_t *fs) {
    test_printf("file_seek");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_RDWR|O_CREAT);
    assert(err == 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs->file_seek(fs, &file, 0, SEEK_SET);
    assert(offset == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = fs->file_read(fs, &file, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(write_buffer, read_buffer) == 0);

    offset = fs->file_seek(fs, &file, 9, SEEK_SET);
    assert(offset == 9);
    memset(read_buffer, 0, sizeof(read_buffer));
    read_length = fs->file_read(fs, &file, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer) - 9);
    assert(strcmp("ABCDEF", read_buffer) == 0);

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_tell(filesystem_t *fs) {
    test_printf("file_tell");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_RDWR|O_CREAT);
    assert(err == 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs->file_seek(fs, &file, 0, SEEK_SET);
    assert(offset == 0);

    off_t pos = fs->file_tell(fs, &file);
    assert(pos == 0);

    offset = fs->file_seek(fs, &file, 0, SEEK_END);
    assert((size_t)offset == strlen(write_buffer));

    pos = fs->file_tell(fs, &file);
    assert((size_t)pos == strlen(write_buffer));

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_size(filesystem_t *fs) {
    test_printf("file_size");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_RDWR|O_CREAT);
    assert(err == 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t size = fs->file_size(fs, &file);
    assert((size_t)size == strlen(write_buffer));

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_file_truncate(filesystem_t *fs) {
    test_printf("file_truncate");

    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_RDWR|O_CREAT);
    assert(err == 0);

    char write_buffer[] = "123456789ABCDEF";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    off_t offset = fs->file_seek(fs, &file, 0, SEEK_SET);
    assert(offset == 0);

    err = fs->file_truncate(fs, &file, 9);
    assert(err == 0);

    char read_buffer[512] = {0};
    ssize_t read_length = fs->file_read(fs, &file, read_buffer, sizeof(read_buffer));
    assert(read_length == 9);
    assert(strcmp(read_buffer, "123456789") == 0);

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_open(filesystem_t *fs) {
    test_printf("dir_open,dir_close");

    fs_dir_t dir;
    int err = fs->dir_open(fs, &dir, "/dir");  // non-exists directory
    assert(err == -ENOTDIR || err == -ENOENT);

    err = fs->mkdir(fs, "/dir", 0777);
    assert(err == 0);

    err = fs->dir_open(fs, &dir, "/dir");
    assert(err == 0);

    err = fs->dir_close(fs, &dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_dir_read(filesystem_t *fs) {
    test_printf("dir_read");

    int err = fs->mkdir(fs, "/dir", 0777);
    assert(err == 0 || err == -EEXIST);

    fs_file_t file;
    err = fs->file_open(fs, &file, "/dir/file", O_WRONLY|O_CREAT);
    assert(err == 0);
    err = fs->file_close(fs, &file);
    assert(err == 0);

    fs_dir_t dir;
    err = fs->dir_open(fs, &dir, "/dir");
    assert(err == 0);

    struct dirent ent;

    if (fs->type != FILESYSTEM_TYPE_FAT) {  // FAT does not return dot entries
        err = fs->dir_read(fs, &dir, &ent);
        assert(err == 0);
        assert(ent.d_type == DT_DIR);
        assert(strcmp(ent.d_name, ".") == 0);

        err = fs->dir_read(fs, &dir, &ent);
        assert(err == 0);
        assert(ent.d_type == DT_DIR);
        assert(strcmp(ent.d_name, "..") == 0);
    }
    err = fs->dir_read(fs, &dir, &ent);
    assert(err == 0);
    assert(ent.d_type == DT_REG);
    assert(strcmp(ent.d_name, "file") == 0);

    err = fs->dir_read(fs, &dir, &ent);  // Reach the end of the directory
    assert(err != 0);

    err = fs->dir_close(fs, &dir);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_remove(filesystem_t *fs) {
    test_printf("remove");

    int err = fs->remove(fs, "/not-exists");
    assert(err == -ENOENT);

    fs_file_t file;
    err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
    assert(err == 0);
    err = fs->file_close(fs, &file);
    assert(err == 0);

    err = fs->remove(fs, "/file");
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_rename(filesystem_t *fs) {
    test_printf("rename");

    int err = fs->rename(fs, "/not-exists", "/renamed");
    assert(err == -ENOENT);

    fs_file_t file;
    err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
    assert(err == 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = fs->file_close(fs, &file);
    assert(err == 0);

    err = fs->rename(fs, "/file", "/renamed");
    assert(err == 0);

    err = fs->file_open(fs, &file, "/renamed", O_RDONLY);
    assert(err == 0);
    char read_buffer[512] = {0};
    ssize_t read_length = fs->file_read(fs, &file, read_buffer, sizeof(read_buffer));
    assert((size_t)read_length == strlen(write_buffer));
    assert(strcmp(read_buffer, write_buffer) == 0);

    err = fs->file_close(fs, &file);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_api_stat(filesystem_t *fs) {
    test_printf("stat");

    // regular file
    fs_file_t file;
    int err = fs->file_open(fs, &file, "/file", O_WRONLY|O_CREAT);
    assert(err == 0);
    char write_buffer[512] = "Hello World!";
    ssize_t write_length = fs->file_write(fs, &file, write_buffer, strlen(write_buffer));
    assert((size_t)write_length == strlen(write_buffer));

    err = fs->file_close(fs, &file);
    assert(err == 0);

    struct stat finfo;
    err = fs->stat(fs, "/file", &finfo);
    assert(err == 0);
    assert((size_t)finfo.st_size == strlen(write_buffer));
    assert(finfo.st_mode & S_IFREG);

    // directory
    err = fs->mkdir(fs, "/dir", 0777);
    assert(err == 0 || err == -EEXIST);
    err = fs->stat(fs, "/dir", &finfo);
    assert(err == 0);
    assert(finfo.st_mode & S_IFDIR);

    printf(COLOR_GREEN("ok\n"));
}

void test_filesystem(void) {
    printf("File system FAT:\n");

    blockdevice_t *heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);

    assert(heap != NULL);
    filesystem_t *fat = filesystem_fat_create();
    assert(fat != NULL);
    setup(heap);

    test_api_format(fat, heap);
    test_api_mount(fat, heap);
    test_api_file_open_close(fat);
    test_api_file_write_read(fat);
    test_api_file_seek(fat);
    test_api_file_tell(fat);
    test_api_file_size(fat);
    test_api_file_truncate(fat);
    test_api_dir_open(fat);
    test_api_dir_read(fat);
    test_api_remove(fat);
    test_api_rename(fat);
    test_api_stat(fat);

    test_api_unmount(fat);
    cleanup(heap);
    filesystem_fat_free(fat);
    blockdevice_heap_free(heap);


    printf("File system littlefs:\n");
    heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    assert(heap != NULL);
    filesystem_t *lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                                   LITTLEFS_LOOKAHEAD_SIZE);
    assert(lfs != NULL);
    setup(heap);

    test_api_format(lfs, heap);
    test_api_mount(lfs, heap);
    test_api_file_open_close(lfs);
    test_api_file_write_read(lfs);
    test_api_file_seek(lfs);
    test_api_file_tell(lfs);
    test_api_file_size(lfs);
    test_api_file_truncate(lfs);
    test_api_dir_open(lfs);
    test_api_dir_read(lfs);
    test_api_remove(lfs);
    test_api_rename(lfs);
    test_api_stat(lfs);

    test_api_unmount(lfs);

    cleanup(heap);
    filesystem_littlefs_free(lfs);
    blockdevice_heap_free(heap);
}
