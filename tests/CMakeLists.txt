set(CMAKE_BUILD_TYPE Debug)

add_executable(tests
  main.c
  test_blockdevice.c
  test_filesystem.c
  test_vfs.c
  test_stdio.c
  test_copy_between_different_filesystems.c
)
if(WITHOUT_BLOCKDEVICE_SD)
  target_compile_definitions(tests PRIVATE WITHOUT_BLOCKDEVICE_SD=1)
endif()
target_compile_options(tests PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(tests PRIVATE
  pico_stdlib
  blockdevice_flash
  blockdevice_heap
  blockdevice_sd
  blockdevice_loopback
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(tests PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(tests)
pico_enable_stdio_usb(tests 1)


add_executable(multicore multicore.c)
if(WITHOUT_BLOCKDEVICE_SD)
  target_compile_definitions(multicore PRIVATE WITHOUT_BLOCKDEVICE_SD=1)
endif()
target_compile_options(multicore PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(multicore PRIVATE
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
pico_enable_stdio_uart(multicore 1)
pico_enable_stdio_usb(multicore 0)
pico_set_binary_type(multicore copy_to_ram)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_tests
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program tests.elf verify reset exit"
    DEPENDS tests
  )
  add_custom_target(run_multicore
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program multicore.elf verify reset exit"
    DEPENDS multicore
  )
endif()
