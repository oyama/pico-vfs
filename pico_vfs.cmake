# For enable default file system

set(PICO_VFS_PATH "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "PICO VFS directory")

function(pico_enable_filesystem TARGET)

  set(options "")
  set(oneValueArgs SIZE AUTO_INIT)
  set(multiValueArgs FS_INIT)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})


  if(ARG_SIZE)
    target_compile_definitions(${TARGET} PRIVATE PICO_FS_DEFAULT_SIZE=${ARG_SIZE})
  endif()

  if(ARG_FS_INIT)
    target_sources(${TARGET} PRIVATE ${ARG_FS_INIT})
  else()
    target_link_libraries(${TARGET} PRIVATE filesystem_default)
  endif()

  if(ARG_AUTO_INIT)
    target_compile_definitions(${TARGET} PRIVATE PICO_FS_AUTO_INIT=1)
  endif()
endfunction()
