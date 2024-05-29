# File system management API

Many functions of pico-vfs are provided as POSIX and C standard file APIs, but the file system management procedures are provided by an API with the prefix `fs_*`. See [STANDARD.md](STANDARD.md) for information on the POSIX and C standard file API.

## `bool fs_init(void)`

File system set-up. The user programme calls `fs_init()` to set up the file system and initialise it to a ready-to-use state.
In addition to explicit execution from the program, the CMake function

```CMakeLists.txt
pico_enable_filesystem(${CMAKE_PROJECT_NAME} AUTO_INIT TRUE)
```
can be specified to automatically execute it with `pre_main()` before `main()`.

## `int fs_format(filesystem_t *fs, blockdevice_t *device)`

Format block devices using file system objects.

## `int fs_mount(const char *path, filesystem_t *fs, blockdevice_t *device)`

Mounts the file system at the specified path.

## `int fs_unmount(const char *path)`

Unmounts the file system at the specified path.

## `int fs_reformat(const char *path)`

Reformat the file system for the specified path.
