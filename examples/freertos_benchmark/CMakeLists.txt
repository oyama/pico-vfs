include(FreeRTOS_Kernel_import.cmake)

add_executable(freertos_benchmark main.c fs_init.c)
target_compile_options(freertos_benchmark PRIVATE -Os)
target_include_directories(freertos_benchmark PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(freertos_benchmark PRIVATE
  FreeRTOS-Kernel
  FreeRTOS-Kernel-Heap1
  pico_stdlib
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(freertos_benchmark PRIVATE -Wl,--print-memory-usage)

#pico_set_binary_type(freertos_benchmark no_flash)
pico_enable_stdio_usb(freertos_benchmark 1)
pico_add_extra_outputs(freertos_benchmark)