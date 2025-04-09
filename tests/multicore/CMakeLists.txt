add_executable(multicore main.c)
target_compile_options(multicore PRIVATE -DPICO_FLASH_ASSUME_CORE1_SAFE=1)
target_link_libraries(multicore PRIVATE
  pico_flash
  pico_stdlib
  pico_multicore
  blockdevice_flash
  blockdevice_heap
  blockdevice_sd
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(multicore PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(multicore)
pico_enable_stdio_usb(multicore 0)
pico_enable_filesystem(multicore FS_INIT fs_init.c)
pico_set_binary_type(multicore copy_to_ram)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_multicore
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program multicore.elf verify reset exit"
    DEPENDS multicore
  )
endif()
