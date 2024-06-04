cmake_minimum_required(VERSION 3.13...3.27)

option(WITHOUT_BLOCKDEVICE_SD "without SD block device in demo and tests")

include(vendor/pico_sdk_import.cmake)
include(pico_vfs.cmake)
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
target_compile_options(blockdevice_sd INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(blockdevice_sd INTERFACE
  hardware_spi
  hardware_sync
)

# Onboard-Flash blockdevice library
add_library(blockdevice_flash INTERFACE)
target_sources(blockdevice_flash INTERFACE
  src/blockdevice/flash.c
)
target_include_directories(blockdevice_flash INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_options(blockdevice_flash INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(blockdevice_flash INTERFACE
  hardware_flash
  hardware_sync
  pico_sync
)

# Heap memory blockdevice library
add_library(blockdevice_heap INTERFACE)
target_sources(blockdevice_heap INTERFACE
  src/blockdevice/heap.c
)
target_include_directories(blockdevice_heap INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_options(blockdevice_heap INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(blockdevice_heap INTERFACE pico_sync)


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
target_compile_options(filesystem_fat INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(filesystem_fat INTERFACE pico_sync)

# littlefs filesystem library
add_library(filesystem_littlefs INTERFACE)
target_sources(filesystem_littlefs INTERFACE
  src/filesystem/littlefs.c
  vendor/littlefs/lfs.c
  vendor/littlefs/lfs_util.c
)
target_include_directories(filesystem_littlefs INTERFACE vendor/littlefs)
target_compile_options(filesystem_littlefs INTERFACE -Wno-unused-function -Wno-null-dereference)
target_link_libraries(filesystem_littlefs INTERFACE pico_sync)

# VFS interface library
add_library(filesystem_vfs INTERFACE)
target_sources(filesystem_vfs INTERFACE src/filesystem/vfs.c)
target_include_directories(filesystem_vfs INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_options(filesystem_vfs INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(filesystem_vfs INTERFACE pico_sync)

# Default file system library
add_library(filesystem_default INTERFACE)
target_sources(filesystem_default INTERFACE src/filesystem/fs_init.c)
target_include_directories(filesystem_default INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_options(filesystem_default INTERFACE -Werror -Wall -Wextra -Wnull-dereference)
target_link_libraries(filesystem_default INTERFACE
  blockdevice_flash
  filesystem_littlefs
  filesystem_vfs
)


add_subdirectory(examples EXCLUDE_FROM_ALL)
add_subdirectory(tests EXCLUDE_FROM_ALL)
