# USB MSC Logger demo
add_executable(logger
  main.c
  usb_msc.c
  usb_descriptors.c
)
target_include_directories(logger PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(logger PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(logger PRIVATE
  pico_stdlib
  hardware_adc
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
  tinyusb_board
  tinyusb_device
)
pico_enable_stdio_usb(logger 1)
pico_add_extra_outputs(logger)
