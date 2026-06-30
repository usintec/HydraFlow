# HydraFlow

Scientific computing and intelligent graphics platform built on HydraCore.

See [HydraFlow/Readme.md](HydraFlow/Readme.md) for full documentation.

## Quick Build

```bash
cd HydraFlow
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build/Debug --parallel
./build/Debug/bin/HydraCore_Tests
```
