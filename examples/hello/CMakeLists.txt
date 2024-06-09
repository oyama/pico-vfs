# Default filesystem init demo
add_executable(hello main.c)
target_link_libraries(hello PRIVATE pico_stdlib)
pico_enable_stdio_usb(hello 1)
pico_add_extra_outputs(hello)

# enable file system
pico_enable_filesystem(hello)
