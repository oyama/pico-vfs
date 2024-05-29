# Supported stdio.h standard file API

pico-vfs provides low-level file IO "system calls", allowing many of the file manipulation APIs of stdio.h to be used.

## System call

| System call | Availability       | Standard                         |
|-------------|--------------------|----------------------------------|
| `close`     | :white_check_mark: | IEEE Std 1003.1-1988 ("POSIX.1") |
| `fstat`     | :white_check_mark: | IEEE Std 1003.1-1988 ("POSIX.1") |
| `lseek`     | :white_check_mark: | IEEE Std 1003.1-1988 ("POSIX.1") |
| `open`      | :white_check_mark: | Version 6 AT&T UNIX              |
| `read`      | :white_check_mark: | IEEE Std 1003.1-1990 ("POSIX.1") |
| `stat`      | :white_check_mark: | IEEE Std 1003.1-1988 ("POSIX.1") |
| `unlink`    | :white_check_mark: | POSIX.1-2008                     |
| `write`     | :white_check_mark: | IEEE Std 1003.1-1990 ("POSIX.1") |

## Input and Output(stdio.h)

Note that many functions from stdio.h are available, but some do not behave as expected. Among the input/output functions that have been tested to work, those related to POSIX and the C standard are as follows:

| Function    | Availability       | Standard                                                              |
|-------------|--------------------|-----------------------------------------------------------------------|
| `clearerr`  | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fflush`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90") and Single UNIX Specification ("SUSv3") |
| `fgetc`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fgetpos`   | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fgets`     | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fgetwc`    | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fgetws`    | :white_check_mark: | IEEE Std 1003.1-2001 ("POSIX.1")                                      |
| `fileno`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fmemopen`  | :white_check_mark: | IEEE Std 1003.1-2008 ("POSIX.1")                                      |
| `fopen`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fprintf`   | :white_check_mark: | ANSI X3.159-1989 ("ANSI C89") and ISO/IEC 9899:1999 ("ISO C99")       |
| `fputc`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fputs`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fputwc`    | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fputws`    | :white_check_mark: | IEEE Std 1003.1-2001 ("POSIX.1")                                      |
| `fread`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `freopen`   | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fscanf`    | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fseek`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fsetpos`   | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `ftell`     | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fwide`     | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fwprintf`  | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `fwrite`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `fwscanf`   | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `getc`      | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `getw`      | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `mkstemp`   | :white_check_mark: | IEEE Std 1003.1-2008 ("POSIX.1")                                      |
| `open_memstream` | :white_check_mark: | IEEE Std 1003.1-2008 ("POSIX.1")                                 |
| `perror`    | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `putc`      | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `remove`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90") and X/Open Portability Guide Issue 4, Version 2 ("XPG4.2") |
| `rename`    | :white_check_mark: | IEEE Std 1003.1-1988 ("POSIX.1")                                      |
| `rewind`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `setbuf`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `setvbuf`   | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `tmpfile`   | :x:                | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `tmpnam`    | :x:                | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `ungetc`    | :white_check_mark: | ISO/IEC 9899:1990 ("ISO C90")                                         |
| `ungetwc`   | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `vfprintf`  | :white_check_mark: | ANSI X3.159-1989 ("ANSI C89") and ISO/IEC 9899:1999 ("ISO C99")       |
| `vfscanf`   | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |
| `vfwscanf`  | :white_check_mark: | ISO/IEC 9899:1999 ("ISO C99")                                         |

For more information see the [Newlib documentation](https://sourceware.org/newlib/libc.html#Stdio).
