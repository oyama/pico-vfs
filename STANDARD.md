# Supported stdio.h standard file API

pico-vfs provides low-level file IO "system calls", allowing many of the file manipulation APIs of stdio.h to be used.

## System call

- `int close(int fildes);`
- `int fstat(int fildes, struct stat *buf);`
- `off_t lseek(int fildes, off_t offset, int whence);`
- `int open(const char *path, int oflag, ...);`
- `ssize_t read(int fildes, void *buf, size_t nbyte);`
- `int stat(const char *restrict path, struct stat *restrict buf);`
- `int unlink(const char *path);`
- `ssize_t write(int fildes, const void *buf, size_t nbyte);`

## Input and Output(stdio.h)

Note that many functions from stdio.h are available, but some do not behave as expected. Among the input/output functions that have been tested to work, those related to POSIX and the C standard are as follows:

- `clearerr`: ok
- `fflush`: ok
- `fgetc`: ok
- `fgetpos`: ok
- `fgets`: ok
- `fgetwc`: ok
- `fgetws`: ok
- `fileno`: ok
- `fmemopen`: ok
- `fopen`: ok
- `fputc`: ok
- `fputs`: ok
- `fputwc`: ok
- `fputws`: ok
- `fread`: ok
- `freopen`: ok
- `fseek`: ok
- `fsetpos`: ok
- `ftell`: ok
- `fwide`: ok
- `fwrite`: ok
- `getc`: ok
- `getw`: ok
- `mkstemp`: ok
- `open_memstream`: ok
- `perror`: ok
- `putc`: ok
- `remove`: ok
- `rename`: ok
- `rewind`: ok
- `setbuf`: ok
- `setvbuf`: ok
- `fprintf`: ok
- `fscanf`: ok
- `fwprintf`: ok
- `fwscanf`: ok
- `tmpfile`: _not support_
- `tmpnam`: _not support_
- `ungetc`: ok
- `ungetwc`: ok
- `vfprintf`: ok
- `vfscnaf`: ok
- `vfwscnaf`: ok

For more information see the [Newlib documentation](https://sourceware.org/newlib/libc.html#Stdio).
