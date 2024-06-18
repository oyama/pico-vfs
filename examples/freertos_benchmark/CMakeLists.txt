include(FreeRTOS_Kernel_import.cmake)

add_executable(freertos_benchmark main.c fs_init.c)
target_compile_options(freertos_benchmark PRIVATE -Os -DPICO_FLASH_ASSUME_CORE1_SAFE=1)
target_include_directories(freertos_benchmark PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(freertos_benchmark PRIVATE
  FreeRTOS-Kernel
  FreeRTOS-Kernel-Heap3
  blockdevice_flash
  blockdevice_sd
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
  pico_stdlib
)
target_link_options(freertos_benchmark PRIVATE -Wl,--print-memory-usage)

# NOTE: To write Flash with SMP, it must be run from RAM
pico_set_binary_type(freertos_benchmark copy_to_ram)

pico_enable_stdio_usb(freertos_benchmark 1)
pico_add_extra_outputs(freertos_benchmark)
