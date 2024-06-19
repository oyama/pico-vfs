add_executable(multicore_logger main.c fs_init.c)
target_compile_options(multicore_logger PRIVATE -O2)
target_link_libraries(multicore_logger PRIVATE
  pico_stdlib
  pico_util
  pico_multicore
  blockdevice_sd
  filesystem_fat
  filesystem_vfs
)
pico_enable_stdio_usb(multicore_logger 1)
pico_enable_filesystem(multicore_logger)
pico_add_extra_outputs(multicore_logger)
