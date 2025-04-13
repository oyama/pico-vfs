cmake_minimum_required(VERSION 3.13...3.27)

include(vendor/pico_sdk_import.cmake)
include(pico_vfs.cmake)
set(FAMILY rp2040)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

project(pico-vfs C CXX ASM)
pico_sdk_init()

# Blockdevice header library
add_library(blockdevice INTERFACE)
target_include_directories(blockdevice INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

# SD Card blockdevice library
add_library(blockdevice_sd INTERFACE)
target_sources(blockdevice_sd INTERFACE
  src/blockdevice/sd.c
)
target_link_libraries(blockdevice_sd INTERFACE
  blockdevice
  hardware_spi
  pico_sync
)

# Onboard-Flash blockdevice library
add_library(blockdevice_flash INTERFACE)
target_sources(blockdevice_flash INTERFACE
  src/blockdevice/flash.c
)
target_link_libraries(blockdevice_flash INTERFACE
  blockdevice
  hardware_exception
  hardware_flash
  pico_sync
  pico_flash
)

# Heap memory blockdevice library
add_library(blockdevice_heap INTERFACE)
target_sources(blockdevice_heap INTERFACE
  src/blockdevice/heap.c
)
target_link_libraries(blockdevice_heap INTERFACE
  blockdevice
  pico_sync
)

# Loopback virtual blockdevice library
add_library(blockdevice_loopback INTERFACE)
target_sources(blockdevice_loopback INTERFACE
  src/blockdevice/loopback.c
)
target_link_libraries(blockdevice_loopback INTERFACE
  blockdevice
  pico_sync
)


# Filesystem header library
add_library(filesystem INTERFACE)
target_include_directories(filesystem INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

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
target_link_libraries(filesystem_fat INTERFACE
  filesystem
  pico_sync
)

# littlefs filesystem library
add_library(filesystem_littlefs INTERFACE)
target_sources(filesystem_littlefs INTERFACE
  src/filesystem/littlefs.c
  vendor/littlefs/lfs.c
  vendor/littlefs/lfs_util.c
)
target_include_directories(filesystem_littlefs INTERFACE vendor/littlefs)
target_compile_options(filesystem_littlefs INTERFACE -DLFS_NO_DEBUG -Wno-unused-function -Wno-null-dereference)
target_link_libraries(filesystem_littlefs INTERFACE
  filesystem
  pico_sync
)


# VFS interface library
add_library(filesystem_vfs INTERFACE)
target_sources(filesystem_vfs INTERFACE src/filesystem/vfs.c)
target_include_directories(filesystem_vfs INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(filesystem_vfs INTERFACE pico_sync)

# Default file system library
add_library(filesystem_default INTERFACE)
target_sources(filesystem_default INTERFACE src/filesystem/fs_init.c)
target_include_directories(filesystem_default INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(filesystem_default INTERFACE
  blockdevice_flash
  filesystem_littlefs
  filesystem_vfs
)


# NOTE: Additional targets are only included in the build of the pico-vfs project root
if("${CMAKE_CURRENT_LIST_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  add_subdirectory(examples EXCLUDE_FROM_ALL)
  add_subdirectory(tests EXCLUDE_FROM_ALL)
endif()
