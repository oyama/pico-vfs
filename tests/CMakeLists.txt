set(CMAKE_BUILD_TYPE Debug)

add_executable(tests
  main.c
  test_flash_fat.c
  test_sd_fat.c
  test_flash_littlefs.c
)

target_link_libraries(tests PRIVATE
  pico_stdlib
  blockdevice_flash
  blockdevice_sd
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)

target_include_directories(tests
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../include
)
#target_compile_options(tests PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
#target_compile_options(tests PRIVATE -DENABLE_TRACE)

pico_add_extra_outputs(tests)
pico_enable_stdio_usb(tests 1)


if(OPENOCD)
  add_custom_target(run_tests
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program tests.elf verify reset exit"
    DEPENDS tests
  )
endif()

