set(CMAKE_BUILD_TYPE Debug)

add_executable(integration
  main.c
  test_blockdevice.c
  test_filesystem.c
  test_vfs.c
  test_standard.c
  test_copy_between_different_filesystems.c
)
target_link_libraries(integration PRIVATE
  pico_stdlib
  blockdevice_flash
  blockdevice_heap
  blockdevice_sd
  blockdevice_loopback
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(integration PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(integration)
pico_enable_stdio_usb(integration 1)


find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_integration
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program integration.elf verify reset exit"
    DEPENDS integration
  )
endif()
