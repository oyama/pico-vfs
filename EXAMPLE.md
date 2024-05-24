# pico-vfs examples

The following source code is attached as a sample of _pico-vfs_.

## Benchmark test

Benchmark tests of write and read speeds for each combination of file system and block device.

[examples/benchmark](examples/benchmark/)

## USB Mass storage class data logger

Two file systems are deployed in the on-board flash memory: itlfs for internal use and FAT file system for public use.
Pico data loggers continue to append data to littlefs and when a USB connection is made, the data is copied to the public FAT file system and published to the host PC via USB MSC.

[examples/usb\_msc\_logger](examples/usb_msc_logger/)

## Building sample code

Firmware can be built from the _CMake_ build directory of _pico-vfs_.

```bash
cd build/
make benchmark
make logger
```

## Default file system

The default file system can be enabled in `CMakeLists.txt`. Don't forget to include `pico_vfs.cmake` beforehand. Of course, you can easily add your own file system initialisation code.

[examples/default\_fs](examples/default_fs/)

## Unit and Integration test code

The attached test code includes unit tests for each of the block device API, file system API and VFS API, as well as integration test code to copy data between different block devices and different file systems.

[tests](tests/)
