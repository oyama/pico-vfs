#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "blockdevice/heap.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

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


static void setup(filesystem_t *fs, blockdevice_t *device) {
    int err = fs_format(fs, device);
    assert(err == 0);
    err = fs_mount("/", fs, device);
    assert(err == 0);
}

static void cleanup(filesystem_t *fs, blockdevice_t *device) {
    (void)fs;
    int err = fs_unmount("/");
    assert(err == 0);
    size_t length = device->size(device);
    device->erase(device, 0, length);
}

static void test_clearerr(void) {
    char buffer[512];
    test_printf("clearerr");

    FILE *fp = fopen("/clearerr", "w+");
    assert(feof(fp) == 0);
    assert(ferror(fp) == 0);
    fprintf(fp, "hello world\n");
    fgets(buffer, sizeof(buffer), fp);
    assert(feof(fp) != 0);
    clearerr(fp);
    assert(feof(fp) == 0);
    fclose(fp);

    fp = fopen("/clearerr", "r");
    fprintf(fp, "can't write stream\n");
    assert(ferror(fp) != 0);
    clearerr(fp);
    assert(ferror(fp) == 0);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fflush(void) {
    test_printf("fflush");

    FILE *fp = fopen("/fflush", "w+");
    fprintf(fp, "hello world");
    int fd = fileno(fp);
    struct stat st;
    int err = fstat(fd, &st);
    assert(err == 0);
    assert(st.st_size == 0);

    fflush(fp);
    err = fstat(fd, &st);
    assert(err == 0);
    assert(st.st_size > 0);

    close(fd);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fgetc(void) {
    test_printf("fgetc");

    FILE *fp = fopen("/fgetc", "w+");
    fprintf(fp, "012AB");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    assert(fgetc(fp) == '0');
    assert(fgetc(fp) == '1');
    assert(fgetc(fp) == '2');
    assert(fgetc(fp) == 'A');
    assert(fgetc(fp) == 'B');
    assert(fgetc(fp) == EOF);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fgetpos(void) {
    test_printf("fgetpos");

    FILE *fp = fopen("/fgetpos", "w+");
    fprintf(fp, "012AB");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    fpos_t pos;
    int err = fgetpos(fp, &pos);
    assert(err == 0);
    assert(pos == 0);

    fseek(fp, 0, SEEK_END);
    err = fgetpos(fp, &pos);
    assert(err == 0);
    assert(pos == strlen("012AB"));

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fgets(void) {
    test_printf("fgets");

    FILE *fp = fopen("/fgets", "w+");
    fprintf(fp, "0123456789\nABCDEF\n");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    char buffer[512];
    char *line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "0123456789\n") == 0);
    assert(strcmp(buffer, "0123456789\n") == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "ABCDEF\n") == 0);
    assert(strcmp(buffer, "ABCDEF\n") == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(line == NULL);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fgetwc(void) {
    test_printf("fgetwc");

    FILE *fp = fopen("/fgetwc", "w+");
    fwprintf(fp, L"WCHAR");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    assert(fgetwc(fp) == L'W');
    assert(fgetwc(fp) == L'C');
    assert(fgetwc(fp) == L'H');
    assert(fgetwc(fp) == L'A');
    assert(fgetwc(fp) == L'R');
    assert(fgetwc(fp) == WEOF);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fgetws(void) {
    test_printf("fgetws");

    FILE *fp = fopen("/fgetws", "w+");
    fwprintf(fp, L"WCHAR\nSTRINGS\n");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    wchar_t buffer[512];
    wchar_t *line = fgetws(buffer, sizeof(buffer), fp);
    assert(wcscmp(line, L"WCHAR\n") == 0);
    assert(wcscmp(buffer, L"WCHAR\n") == 0);
    line = fgetws(buffer, sizeof(buffer), fp);
    assert(wcscmp(line, L"STRINGS\n") == 0);
    assert(wcscmp(buffer, L"STRINGS\n") == 0);

    line = fgetws(buffer, sizeof(buffer), fp);
    assert(line == NULL);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fileno(void) {
    test_printf("fileno");

    FILE *fp = fopen("/fileno", "w+");
    fprintf(fp, "hello world\n");
    fflush(fp);
    int fd = fileno(fp);
    lseek(fd, 0, SEEK_SET);
    char buffer[512] = {0};
    ssize_t len = read(fd, buffer, sizeof(buffer));
    assert(len == strlen("hello world\n"));
    assert(strcmp(buffer, "hello world\n") == 0);

    close(fd);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fmemopen(void) {
    test_printf("fmemopen");

    char buffer[512] = {0};
    FILE *fp = fmemopen(buffer, sizeof(buffer), "w+");
    assert(fprintf(fp, "0123456789\n") == 11);
    assert(fprintf(fp, "ABCDEF\n") == 7);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    char read_buffer[512];
    char *line = fgets(read_buffer, sizeof(read_buffer), fp);
    assert(strcmp(line, "0123456789\n") == 0);
    assert(strcmp(read_buffer, "0123456789\n") == 0);

    line = fgets(read_buffer, sizeof(read_buffer), fp);
    assert(strcmp(line, "ABCDEF\n") == 0);
    assert(strcmp(read_buffer, "ABCDEF\n") == 0);

    line = fgets(read_buffer, sizeof(read_buffer), fp);
    assert(line == NULL);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fopen(void) {
    test_printf("fopen");

    FILE *fp = fopen("/fopen-not-exists", "r");
    assert(fp == NULL);
    assert(errno == ENOENT);

    fp = fopen("/fopen", "w");
    assert(fp != NULL);
    assert(fprintf(fp, "0123456789\n") == 11);
    assert(fprintf(fp, "ABCDEF\n") == 7);
    int err = fclose(fp);
    assert(err == 0);

    fp = fopen("/fopen", "r");
    assert(fp != NULL);
    char buffer[512];
    char *line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "0123456789\n") == 0);
    assert(strcmp(buffer, "0123456789\n") == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "ABCDEF\n") == 0);
    assert(strcmp(buffer, "ABCDEF\n") == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(line == NULL);

    err = fclose(fp);
    assert(err == 0);

    fp = fopen("/fopen", "a+");
    assert(fp != NULL);
    assert(ftell(fp) > 0);
    fprintf(fp, "APPEND\n");
    fseek(fp, 0, SEEK_SET);
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "0123456789\n") == 0);
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "ABCDEF\n") == 0);
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "APPEND\n") == 0);
    err = fclose(fp);
    assert(err == 0);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fputc(void) {
    test_printf("fputc");

    FILE *fp = fopen("/fputc", "w+");
    assert(fputc('A', fp) == 'A');
    assert(fputc('B', fp) == 'B');
    assert(fputc('C', fp) == 'C');

    fseek(fp, 0, SEEK_SET);
    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "ABC") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fputs(void) {
    test_printf("fputs");

    FILE *fp = fopen("/fputs", "w+");
    int err = fputs("Hello\n", fp);
    assert(err == 0);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "Hello\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fputwc(void) {
    test_printf("fputwc");

    FILE *fp = fopen("/fputwc", "w+");
    assert(fputwc(L'W', fp) == L'W');
    assert(fputwc(L'C', fp) == L'C');

    fseek(fp, 0, SEEK_SET);
    wchar_t buffer[512];
    fgetws(buffer, sizeof(buffer), fp);
    assert(wcscmp(buffer, L"WC") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fputws(void) {
    test_printf("fputws");

    FILE *fp = fopen("/fputws", "w+");
    assert(fputws(L"WIDE CHAR\n", fp) == 0);

    fseek(fp, 0, SEEK_SET);
    wchar_t buffer[512];
    fgetws(buffer, sizeof(buffer), fp);
    assert(wcscmp(buffer, L"WIDE CHAR\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fread(void) {
    test_printf("fread");

    FILE *fp = fopen("/fread", "w+");
    fprintf(fp, "Hello World\n");
    fseek(fp, 0, SEEK_SET);

    char buffer[512] = {0};
    size_t size = fread(buffer, 1, sizeof(buffer), fp);
    assert(size == strlen("Hello World\n"));
    assert(strcmp(buffer, "Hello World\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_freopen(void) {
    test_printf("freopen");

    FILE *fp = fopen("/freopen", "w");
    fprintf(fp, "Hello World\n");

    fp = freopen("/freopen", "r", fp);
    assert(fp != NULL);
    char buffer[512];
    char *line = fgets(buffer, sizeof(buffer), fp);
    assert(fp != NULL);
    assert(strcmp(line, "Hello World\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fseek(void) {
    test_printf("fseek");

    FILE *fp = fopen("/fseek", "w+");
    fprintf(fp, "Hello\nWorld\n");

    assert(fseek(fp, 6, SEEK_SET) == 0);

    char buffer[512];
    char *line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "World\n") == 0);

    assert(fseek(fp, 0, SEEK_SET) == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "Hello\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fsetpos(void) {
    test_printf("fsetpos");

    FILE *fp = fopen("/fsetpos", "w+");
    fprintf(fp, "Hello\nWorld\n");

    fpos_t pos = 6;
    assert(fsetpos(fp, &pos) == 0);

    char buffer[512];
    char *line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "World\n") == 0);

    pos = 0;
    assert(fsetpos(fp, &pos) == 0);

    line = fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(line, "Hello\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_ftell(void) {
    test_printf("ftell");

    FILE *fp = fopen("/ftell", "w+");
    fprintf(fp, "Hello\nWorld\n");
    fseek(fp, 0, SEEK_SET);
    assert(ftell(fp) == 0);

    fseek(fp, 6, SEEK_SET);
    assert(ftell(fp) == 6);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fwide(void) {
    test_printf("fwide");

    FILE *fp = fopen("/fwide", "w");
    assert(fwide(fp, 0) == 0);
    fprintf(fp, "byte\n");
    assert(fwide(fp, 0) < 0);

    fp = freopen("/fwide", "w", fp);
    assert(fwide(fp, 0) == 0);
    assert(fwide(fp, 1) > 0);
    fclose(fp);

    fp = fopen("/fwide.wide", "w");
    assert(fwide(fp, 0) == 0);
    fwprintf(fp, L"wide\n");
    assert(fwide(fp, 0) > 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_fwrite(void) {
    test_printf("fwrite");

    FILE *fp = fopen("/fwrite", "w+");
    uint8_t write_buffer[] = {0x01, 0x02, 0x03, 0x04};
    size_t size = fwrite(write_buffer, 1, sizeof(write_buffer), fp);
    assert(size == sizeof(write_buffer));

    fseek(fp, 0, SEEK_SET);

    uint8_t read_buffer[4] = {0};
    size = fread(read_buffer, 1, sizeof(read_buffer), fp);
    assert(size == sizeof(read_buffer));
    assert(memcmp(write_buffer, read_buffer, sizeof(write_buffer)) == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_getc(void) {
    test_printf("getc");

    FILE *fp = fopen("/getc", "w+");
    fprintf(fp, "012AB");
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    assert(getc(fp) == '0');
    assert(getc(fp) == '1');
    assert(getc(fp) == '2');
    assert(getc(fp) == 'A');
    assert(getc(fp) == 'B');
    assert(getc(fp) == EOF);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_getw(void) {
    test_printf("getw");

    FILE *fp = fopen("/getw", "w+");
    int word = 536870911;
    fwrite(&word, 1, sizeof(word), fp);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    assert(getw(fp) == word);
    assert(getw(fp) == EOF);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

static void test_mkstemp(void) {
    test_printf("mkstemp");

    FILE *fp = fopen("/mkstemp.000000", "w");
    fprintf(fp, "exists\n");
    fclose(fp);

    char path[] = "/mkstemp.XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);

    printf(COLOR_GREEN("ok\n"));
}

static void test_open_memstream(void) {
    test_printf("open_memstream");

    char *buffer = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&buffer, &size);
    fprintf(fp, "hello world");
    fclose(fp);

    assert(size == strlen("hello world"));
    assert(memcmp(buffer, "hello world", sizeof("hello world")) == 0);
    free(buffer);

    printf(COLOR_GREEN("ok\n"));
}

static void test_perror(void) {
    test_printf("perror");

    errno = 0;
    perror(COLOR_GREEN("ok"));
}

static void test_putc(void) {
    test_printf("putc");

    FILE *fp = fopen("/putc", "w+");
    assert(putc('A', fp) == 'A');
    assert(putc('B', fp) == 'B');
    assert(putc('C', fp) == 'C');

    fseek(fp, 0, SEEK_SET);
    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "ABC") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_remove(void) {
    test_printf("remove");

    FILE *fp = fopen("/remove", "w");
    fclose(fp);

    int err = remove("/remove");
    assert(err == 0);

    err = remove("/not-exists");
    assert(err == -1);
    assert(errno == ENOENT);

    printf(COLOR_GREEN("ok\n"));
}

void test_rename(void) {
    test_printf("rename");

    FILE *fp = fopen("/rename", "w");
    fclose(fp);

    int err = rename("/rename", "/renamed");
    assert(err == 0);

    err = rename("/not-exists", "/renamed");
    assert(err == -1);
    assert(errno == ENOENT);

    printf(COLOR_GREEN("ok\n"));
}

void test_rewind(void) {
    test_printf("rewind");

    FILE *fp = fopen("/rewind", "w+");
    fprintf(fp, "Hello World");
    rewind(fp);
    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "Hello World") == 0);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_setbuf(void) {
    test_printf("setbuf");

    char buffer[BUFSIZ] = {0};

    FILE *fp = fopen("/setbuf", "w");
    setbuf(fp, buffer);
    fprintf(fp, "Hello World\n");
    fclose(fp);
    assert(strcmp(buffer, "Hello World\n") == 0);

    printf(COLOR_GREEN("ok\n"));
}

void test_setvbuf(void) {
    test_printf("setvbuf");

    char buffer[BUFSIZ];

    // Do not use a buffer
    FILE *fp = fopen("/setvbuf", "w+");
    int fd = fileno(fp);
    int err = setvbuf(fp, NULL, _IONBF, 0);
    assert(err == 0);
    fprintf(fp, "Hello World");
    char read_buffer[512] = {0};

    lseek(fd, 0, SEEK_SET);
    ssize_t size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == strlen("Hello World"));

    close(fd);
    fclose(fp);

    // use full output buffering
    fp = fopen("/setvbuf", "w+");
    fd = fileno(fp);
    err = setvbuf(fp, buffer, _IOFBF, 5);
    assert(err == 0);
    fprintf(fp, "He");
    lseek(fd, 0, SEEK_SET);
    size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == 0);

    fprintf(fp, "llo W");
    lseek(fd, 0, SEEK_SET);
    size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == strlen("Hello"));

    fflush(fp);

    lseek(fd, 0, SEEK_SET);
    size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == strlen("Hello W"));

    close(fd);
    fclose(fp);

    // use line buffering
    fp = fopen("/setvbuf", "w+");
    fd = fileno(fp);
    err = setvbuf(fp, buffer, _IOLBF, 128);
    assert(err == 0);
    fprintf(fp, "Hello");
    lseek(fd, 0, SEEK_SET);
    size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == 0);

    fprintf(fp, " World\n");

    lseek(fd, 0, SEEK_SET);
    size = read(fd, read_buffer, sizeof(read_buffer));
    assert(size == strlen("Hello World\n"));

    close(fd);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_fprintf(void) {
    test_printf("fprintf");

    FILE *fp = fopen("/fprintf", "w+");
    fprintf(fp, "%s", "Hello World");
    rewind(fp);
    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "Hello World") == 0);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_fscanf(void) {
    test_printf("fscanf");

    FILE *fp = fopen("/fscanf", "w+");
    fprintf(fp, "%s", "123,abc,0.987\n");
    rewind(fp);
    int int_value = 0;
    char strings[512];
    float float_value = 0.0;

    int assigned = fscanf(fp, "%d,%[^,],%f\n", &int_value, strings, &float_value);
    assert(assigned == 3);
    assert(int_value == 123);
    assert(strcmp(strings, "abc") == 0);
    assert(float_value < 1.0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_fwprintf(void) {
    test_printf("fwprintf");

    FILE *fp = fopen("/fwprintf", "w+");
    int size = fwprintf(fp, L"Hello World\n");
    assert(size == 12);

    fflush(fp);
    rewind(fp);
    wchar_t buffer[512];
    wchar_t *line = fgetws(buffer, sizeof(buffer), fp);
    assert(wcscmp(line, L"Hello World\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_fwscanf(void) {
    test_printf("fwscanf");

    FILE *fp = fopen("/fwscanf", "w+");
    fwprintf(fp, L"123,abc,0.987\n");
    rewind(fp);
    int int_value = 0;
    char strings[512];
    float float_value = 0.0;

    int assigned = fwscanf(fp, L"%d,%[^,],%f\n", &int_value, strings, &float_value);
    assert(assigned == 3);
    assert(int_value == 123);
    assert(strcmp(strings, "abc") == 0);
    assert(float_value < 1.0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_tmpfile(void) {
    test_printf("tmpfile");

    FILE *fp = tmpfile();
    assert(fp == NULL);

    printf("not support\n"); // tmpfile also requires the  global pointr environ
}

void test_tmpnam(void) {
    test_printf("tmpnam");

    char *path = tmpnam("prefix");
    assert(path != NULL);
    assert(strcmp(path, "prefix") == 0); // unintended behavior

    printf("not support\n"); // The global pointer environ is also required
}

void test_ungetc(void) {
    test_printf("ungetc");

    FILE *fp = fopen("/ungetc", "w+");
    fprintf(fp, "12345");
    fflush(fp);

    rewind(fp);
    assert(fgetc(fp) == '1');
    assert(fgetc(fp) == '2');
    assert(fgetc(fp) == '3');
    ungetc('3', fp);
    assert(fgetc(fp) == '3');
    assert(fgetc(fp) == '4');

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}


void test_ungetwc(void) {
    test_printf("ungetwc");

    FILE *fp = fopen("/ungetc", "w+");
    fwprintf(fp, L"12345");
    fflush(fp);

    rewind(fp);
    assert(fgetwc(fp) == L'1');
    assert(fgetwc(fp) == L'2');
    assert(fgetwc(fp) == L'3');
    ungetwc(L'3', fp);
    assert(fgetwc(fp) == L'3');
    assert(fgetwc(fp) == L'4');

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

int _test_vfprintf(FILE *fp, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int size = vfprintf(fp, format, args);
    va_end(args);
    return size;
}

void test_vfprintf(void) {
    test_printf("vfprintf");

    FILE *fp = fopen("/vfprintf", "w+");

    int size = _test_vfprintf(fp, "%s %s\n", "Hello", "World");
    assert(size == 12);

    fflush(fp);
    rewind(fp);
    char buffer[512];
    fgets(buffer, sizeof(buffer), fp);
    assert(strcmp(buffer, "Hello World\n") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}


int _test_vfscanf(FILE *fp, const char *format, ...) {
    va_list list;
    va_start(list, format);
    int size = vfscanf(fp, format, list);
    va_end(list);
    return size;
}

void test_vfscanf(void) {
    test_printf("vfscnaf");

    FILE *fp = fopen("/vfscanf", "w+");
    fprintf(fp, "123,ABC\n");
    fflush(fp);
    rewind(fp);

    int int_value = 0;
    char char_value[512];
    int number = _test_vfscanf(fp, "%d,%s\n", &int_value, char_value);
    assert(number == 2);
    assert(int_value == 123);
    assert(strcmp(char_value, "ABC") == 0);
    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

int _test_vfwscanf(FILE *fp, const wchar_t *format, ...) {
    va_list list;
    va_start(list, format);
    int size = vfwscanf(fp, format, list);
    va_end(list);
    return size;
}

void test_vfwscanf(void) {
    test_printf("vfwscnaf");

    FILE *fp = fopen("/vfwscanf", "w+");
    fwprintf(fp, L"123,ABC");
    fflush(fp);
    rewind(fp);

    int int_value = 0;
    char wchar_value[512] = {0};
    int number = _test_vfwscanf(fp, L"%d,%s", &int_value, wchar_value);
    assert(number == 2);
    assert(int_value == 123);
    assert(strcmp(wchar_value, "ABC") == 0);

    fclose(fp);

    printf(COLOR_GREEN("ok\n"));
}

void test_standard_file_api(void) {
    test_clearerr();
    test_fflush();
    test_fgetc();
    test_fgetpos();
    test_fgets();
    test_fgetwc();
    test_fgetws();
    test_fileno();
    test_fmemopen();
    test_fopen();
    test_fputc();
    test_fputs();
    test_fputwc();
    test_fputws();
    test_fread();
    test_freopen();
    test_fseek();
    test_fsetpos();
    test_ftell();
    test_fwide();
    test_fwrite();
    test_getc();
    //test_getdelim();  // not support
    //test_getline();  // not support
    test_getw();
    //test_mktemp();  // NOTE: `mktemp' is dangerous; use `mkstemp' instead
    test_mkstemp();
    test_open_memstream();
    test_perror();
    test_putc();
    //test_putc_unlocked();
    test_remove();
    test_rename();
    test_rewind();
    test_setbuf();
    test_setvbuf();
    test_fprintf();
    test_fscanf();
    test_fwprintf();
    test_fwscanf();
    test_tmpfile();
    test_tmpnam();
    test_ungetc();
    test_ungetwc();
    test_vfprintf();
    test_vfscanf();
    test_vfwscanf();
}

void test_standard(void) {
    printf("POSIX and C standard file API(littlefs):\n");

    blockdevice_t *heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    filesystem_t *lfs = filesystem_littlefs_create(LITTLEFS_BLOCK_CYCLE,
                                                   LITTLEFS_LOOKAHEAD_SIZE);

    setup(lfs, heap);

    test_standard_file_api();

    cleanup(lfs, heap);
    filesystem_littlefs_free(lfs);
    blockdevice_heap_free(heap);


    printf("POSIX and C standard file API(FAT):\n");

    heap = blockdevice_heap_create(HEAP_STORAGE_SIZE);
    filesystem_t *fat = filesystem_fat_create();
    setup(fat, heap);

    test_standard_file_api();

    cleanup(fat, heap);
    filesystem_fat_free(fat);
    blockdevice_heap_free(heap);
}
