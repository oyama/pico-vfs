# USB MSC Logger demo
add_executable(usb_logger
  main.c
  usb_msc.c
  usb_descriptors.c
)
target_include_directories(usb_logger PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(usb_logger PRIVATE
  pico_stdlib
  hardware_adc
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
  tinyusb_board
  tinyusb_device
)
pico_enable_stdio_usb(usb_logger 1)
pico_add_extra_outputs(usb_logger)
