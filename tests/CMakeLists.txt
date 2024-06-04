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
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(tests PRIVATE -Wl,--print-memory-usage)

pico_add_extra_outputs(tests)
pico_enable_stdio_usb(tests 1)
