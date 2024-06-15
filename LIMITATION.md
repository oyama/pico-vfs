# Limitations of pico-vfs

When using pico-vfs, there are several limitations and behavior specifics that users should be aware of. These limitations are particularly important when performing multicore operations.

## File System Limitations

1. **Multiple Open Instances**: The current implementations of littlefs and FatFs do not support opening the same file multiple times simultaneously[^1][^2]. This limitation can affect scenarios where multiple accesses to the same file are required concurrently.

2. **`core1` Usage with Onboard Flash Block Device**: When operating file systems using the onboard flash block device on `core1`, it is necessary to run the operations from RAM[^3].

3. **Memory map IO**: `mmap` system call not supported.

We recommend reviewing these limitations before designing systems that heavily rely on multicore operations or require high file access availability.

## References

[^1]: [littlefs Issue#483](https://github.com/littlefs-project/littlefs/issues/483)
[^2]: [FatFs Module Application Note](http://elm-chan.org/fsw/ff/doc/appnote.html#dup)
[^3]: [Raspberry Pi Pico Forum](https://forums.raspberrypi.com/viewtopic.php?t=311709)
