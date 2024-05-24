# For enable default file system

set(PICO_VFS_PATH "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "PICO VFS directory")

function(pico_enable_filesystem TARGET)
  set(options "")
  set(oneValueArgs SIZE)
  set(multiValueArgs FS_INIT)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(ARG_SIZE)
    target_compile_definitions(${TARGET} PRIVATE DEFAULT_FS_SIZE=${ARG_SIZE})
  endif()

  if(ARG_FS_INIT)
    target_sources(${TARGET} PRIVATE ${ARG_FS_INIT})
  else()
    target_link_libraries(${TARGET} PRIVATE filesystem_default)
  endif()
endfunction()
