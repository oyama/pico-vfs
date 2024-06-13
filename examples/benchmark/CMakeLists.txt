# Benchmark demo
add_executable(benchmark main.c)
target_compile_options(benchmark PRIVATE -O2 -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(benchmark PRIVATE
  pico_stdlib
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
pico_enable_stdio_usb(benchmark 1)
pico_add_extra_outputs(benchmark)
