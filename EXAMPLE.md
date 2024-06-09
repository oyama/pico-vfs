# Example of pico-vfs library

## Examples

| App             | Description                                                             |
|-----------------|-------------------------------------------------------------------------|
| [hello](hello)  | Hello filesystem world.                                                 |
| [fs\_inits](fs_inits) | Examples of file system layout combinations.                      |
| [benchmark](benchmark)| Data transfer tests with different block devices and different file system combinations.|
| [usb\_msc\_logger](usb_msc_logger) | Data logger that mounts littlefs and FAT on flash memory and shares it with a PC via USB mass storage class.|


## Building sample code

Firmware can be built from the _CMake_ build directory of _pico-vfs_.

```bash
cd build/
make hello fs_init_example benchmark logger
```
