set(CMAKE_BUILD_TYPE Debug)

add_executable(unittests
  main.c
  test_blockdevice.c
  test_filesystem.c
  test_vfs.c
  test_standard.c
  test_copy_between_different_filesystems.c
)
target_link_libraries(unittests PRIVATE
  pico_stdlib
  blockdevice_heap
  blockdevice_loopback
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(unittests PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(unittests)
pico_enable_stdio_usb(unittests 1)


add_subdirectory(integration)
add_subdirectory(multicore)
add_subdirectory(large_file)
add_subdirectory(host)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_unittests
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program unittests.elf verify reset exit"
    DEPENDS unittests
  )
endif()
