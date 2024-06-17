# Tests

The project has four tests:

1. unit tests `unittests` using heap memory
2. integration tests `integrate` using Raspberry Pi Pico hardware
3. multi-core tests `multicore`
4. unit tests `host` on the host

```bash
cd build
make unittests integration multicore
make run_unittests
make run_integration
make run_multicore
```

```bash
cd build
cmake -DPICO_PLATFORM=host
make host
./tests/host/host
```
