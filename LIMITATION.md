# Limitations of pico-vfs

When using pico-vfs, there are several limitations and behavior specifics that users should be aware of. These limitations are particularly important when performing multicore operations.

## File System Limitations

1. **Multiple Open Instances**: The current implementations of littlefs and FatFs do not support opening the same file multiple times simultaneously[^1][^2]. This limitation can affect scenarios where multiple accesses to the same file are required concurrently.

2. **Access to on-board flash from `core1`**: When operating file systems using the onboard flash block device on `core1`, it is necessary to run the operations from RAM[^3].

3. **Access on-board flash in FreeRTOS**: To access the flash device, the firmware must be stored in RAM or run with `#define configNUMBER_OF_CORES 1`.

4. **Memory maped IO**: `mmap` system call not supported.

5. **Max File Size for FAT**: The maximum single file size of a FAT file system depends on the capacity of the storage medium. Check the size of the SD card used and the type of FAT (FAT16/32/ExFat) automatically assigned.

We recommend reviewing these limitations before designing systems that heavily rely on multicore operations or require high file access availability.

## References

[^1]: [littlefs Issue#483](https://github.com/littlefs-project/littlefs/issues/483)
[^2]: [FatFs Module Application Note](http://elm-chan.org/fsw/ff/doc/appnote.html#dup)
[^3]: [Raspberry Pi Pico Forum](https://forums.raspberrypi.com/viewtopic.php?t=311709)
