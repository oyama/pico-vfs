set(CMAKE_BUILD_TYPE Debug)

add_executable(large_file main.c)
target_link_libraries(large_file PRIVATE
  pico_stdlib
  blockdevice_sd
  filesystem_fat
  filesystem_vfs
)
target_link_options(large_file PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(large_file)
pico_enable_stdio_usb(large_file 1)


find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_large_file
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program large_file.elf verify reset exit"
    DEPENDS large_file
  )
endif()
