cmake_minimum_required(VERSION 3.13...3.27)

include(vendor/pico_sdk_import.cmake)
set(FAMILY rp2040)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

project(pico-vfs C CXX ASM)
pico_sdk_init()

# SD Card blockdevice library
add_library(blockdevice_sd INTERFACE)
target_sources(blockdevice_sd INTERFACE
  src/blockdevice/sd.c
)
target_include_directories(blockdevice_sd INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(blockdevice_sd INTERFACE hardware_spi)

# Onboard-Flash blockdevice library
add_library(blockdevice_flash INTERFACE)
target_sources(blockdevice_flash INTERFACE
  src/blockdevice/flash.c
)
target_include_directories(blockdevice_flash INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(blockdevice_flash INTERFACE
  hardware_flash
  hardware_sync
)

# FAT filesystem library
add_library(filesystem_fat INTERFACE)
target_sources(filesystem_fat INTERFACE
  src/filesystem/fat.c
  vendor/ff15/source/ff.c
  vendor/ff15/source/ffsystem.c
  vendor/ff15/source/ffunicode.c
)
target_include_directories(filesystem_fat INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  include/filesystem/ChaN
  vendor/ff15/source
)

# littlefs filesystem library
add_library(filesystem_littlefs INTERFACE)
target_sources(filesystem_littlefs INTERFACE
  src/filesystem/littlefs.c
  vendor/littlefs/lfs.c
  vendor/littlefs/lfs_util.c
)
target_include_directories(filesystem_littlefs INTERFACE vendor/littlefs)
target_compile_options(filesystem_littlefs INTERFACE -Wno-unused-function -Wno-null-dereference)

# VFS interface library
add_library(filesystem_vfs INTERFACE)
target_sources(filesystem_vfs INTERFACE src/filesystem/vfs.c)
target_include_directories(filesystem_vfs INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)


add_executable(benchmark EXCLUDE_FROM_ALL benchmark.c)
target_include_directories(benchmark
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_options(benchmark PRIVATE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(benchmark PRIVATE
  pico_stdlib
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
target_link_options(benchmark PRIVATE -Wl,--print-memory-usage)
pico_enable_stdio_usb(benchmark 1)
pico_add_extra_outputs(benchmark)



add_executable(demo EXCLUDE_FROM_ALL main.c)
target_include_directories(demo
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(demo PRIVATE
  pico_stdlib
  blockdevice_sd
  blockdevice_flash
  filesystem_fat
  filesystem_littlefs
  filesystem_vfs
)
pico_enable_stdio_usb(demo 1)
pico_add_extra_outputs(demo)


add_subdirectory(tests EXCLUDE_FROM_ALL)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(run_benchmark
    COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program benchmark.elf verify reset exit"
    DEPENDS benchmark
  )
  add_custom_target(reset COMMAND ${OPENOCD} -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c init -c reset -c exit)
endif()
