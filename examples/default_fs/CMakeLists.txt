# Default filesystem init demo
add_executable(default_fs main.c)
target_link_libraries(default_fs PRIVATE pico_stdlib)
pico_enable_stdio_usb(default_fs 1)
pico_add_extra_outputs(default_fs)

# enable file system
pico_enable_filesystem(default_fs)

# Additional sources of initialisation functions can be added to customise the file system configuration.
#
# pico_enable_filesystem(default_fs FS_INIT my_fs_init.c)
# target_link_libraries(default_fs PRIVATE
#   pico_stdlib
#   blockdevice_flash
#   blockdevice_sd
#   filesystem_littlefs
#   filesystem_fat
#   filesystem_vfs
# )
