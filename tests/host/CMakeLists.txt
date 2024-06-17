set(CMAKE_BUILD_TYPE Debug)

add_executable(host
  main.c
  test_blockdevice.c
  test_filesystem.c
  test_benchmark.c
)
target_compile_options(pico_stdio INTERFACE -Wno-error=unused-parameter -Wno-error=sign-compare)
target_compile_options(host PRIVATE
  -Wno-incompatible-function-pointer-types
  -Wno-incompatible-pointer-types
)
target_link_libraries(host PRIVATE
  pico_stdlib
  blockdevice_heap
  filesystem_fat
  filesystem_littlefs
)
