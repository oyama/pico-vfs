#
# File system enable and customise function
#

if (NOT PICO_VFS_PATH)
  set(PICO_VFS_PATH "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "PICO VFS directory")
endif()

function(pico_enable_filesystem TARGET)
  set(options "")
  set(oneValueArgs SIZE AUTO_INIT MAX_FAT_VOLUME MAX_MOUNTPOINT)
  set(multiValueArgs FS_INIT)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Default file system size in bytes
  if(ARG_SIZE)
    target_compile_definitions(${TARGET} PRIVATE PICO_FS_DEFAULT_SIZE=${ARG_SIZE})
  endif()

  # Add custom fs_init.c source files
  if(ARG_FS_INIT)
    target_sources(${TARGET} PRIVATE ${ARG_FS_INIT})
  else()
    target_link_libraries(${TARGET} PRIVATE filesystem_default)
  endif()

  # Enable automatic execution of fs_init()
  if(ARG_AUTO_INIT)
    target_compile_definitions(${TARGET} PRIVATE PICO_FS_AUTO_INIT=1)
  endif()

  # Maximum number of volumes in a FAT file system
  if(ARG_MAX_FAT_VOLUME)
    target_compile_definitions(${TARGET} PRIVATE PICO_VFS_MAX_FAT_VOLUME=${ARG_MAX_FAT_VOLUME})
  else()
    target_compile_definitions(${TARGET} PRIVATE PICO_VFS_MAX_FAT_VOLUME=4)
  endif()

  # Maximum number of file system mount points
  if(ARG_MAX_MOUNTPOINT)
    target_compile_definitions(${TARGET} PRIVATE PICO_VFS_MAX_MOUNTPOINT=${ARG_MAX_MOUNTPOINT})
  else()
    target_compile_definitions(${TARGET} PRIVATE PICO_VFS_MAX_MOUNTPOINT=8)
  endif()
endfunction()
