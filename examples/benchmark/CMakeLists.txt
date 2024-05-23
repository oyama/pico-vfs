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
target_link_options(benchmark PRIVATE -Wl,--print-memory-usage)
if(WITHOUT_BLOCKDEVICE_SD)
  target_compile_definitions(benchmark PRIVATE WITHOUT_BLOCKDEVICE_SD=1)
endif()
pico_enable_stdio_usb(benchmark 1)
pico_add_extra_outputs(benchmark)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_benchmark
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program benchmark.elf verify reset exit"
    DEPENDS benchmark
  )
endif()
