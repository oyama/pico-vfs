add_executable(fs_init_example main.c)
target_compile_options(fs_init_example PRIVATE -Os)
target_link_libraries(fs_init_example PRIVATE
  pico_stdlib
  blockdevice_flash
  blockdevice_heap
  blockdevice_loopback
  blockdevice_sd
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
pico_enable_filesystem(fs_init_example FS_INIT fs_init_littlefs.c)
#pico_enable_filesystem(fs_init_example FS_INIT fs_init_fat.c)
#pico_enable_filesystem(fs_init_example FS_INIT fs_init_fat_sdcard.c)
#pico_enable_filesystem(fs_init_example FS_INIT fs_init_littlefs_sdcard.c)
#pico_enable_filesystem(fs_init_example FS_INIT fs_init_heap.c)
#pico_enable_filesystem(fs_init_example FS_INIT fs_init_loopback.c)

pico_enable_stdio_usb(fs_init_example 1)
pico_add_extra_outputs(fs_init_example)
