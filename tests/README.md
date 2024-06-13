# Tests

The project has three tests:

1. unit tests `unittests` using heap memory
2. integration tests `integrate` using Raspberry Pi Pico hardware
3. `multicore` using multi-core

```bash
cd build
make unittests integration multicore
make run_unittests
make run_integration
make run_multicore
```
