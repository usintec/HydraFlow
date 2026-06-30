---
name: HydraCore Build Setup
description: How HydraCore is built in this Replit environment — deps, build commands, test runner
---

## Dependencies (all via Nix system packages)
- spdlog, nlohmann_json, yaml-cpp, gtest — installed via installSystemDependencies
- cmake, gcc, ninja, clang, clang-tools — build toolchain
- GLM: NOT available as `glm` in nixpkgs; skipped (handled gracefully in CMake)

## Build commands
```bash
cd HydraFlow
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build/Debug --parallel
./build/Debug/bin/HydraCore_Tests        # 29 tests
./build/Debug/bin/HydraCore_BasicApp     # example
```

**Why:** GLM was absent from Nix rippkgs index under name `glm`; CMake finds it via `find_package(glm QUIET)` and silently skips if absent — no hard error.

**How to apply:** When adding GLM headers, vendor them into third_party/ or check nixpkgs for the correct attribute name at that time.
