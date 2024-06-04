# Benchmark demo
add_executable(benchmark main.c)
target_compile_options(benchmark PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(benchmark PRIVATE
  pico_stdlib
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
if(WITHOUT_BLOCKDEVICE_SD)
  target_compile_definitions(benchmark PRIVATE WITHOUT_BLOCKDEVICE_SD=1)
endif()
pico_enable_stdio_usb(benchmark 1)
pico_add_extra_outputs(benchmark)
