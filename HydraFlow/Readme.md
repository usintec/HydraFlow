# HydraFlow

HydraFlow is an open, modular scientific computing and intelligent graphics platform.

**HydraFlow is NOT a rendering engine.** It is a scientific computing framework that uses Ogre3D only as its rendering backend.

---

## Architecture

| Layer | Responsibility |
|---|---|
| **HydraCore** | Standalone foundation library — no rendering, no AI, no GPU |
| **Ogre3D** (future) | Rendering backend only |
| **HydraModules** (future) | Scientific simulation, GPU compute, AI integration |

---

## HydraCore — Module 1: Application

The first module provides the full application lifecycle framework.

### Components

| Class | Role |
|---|---|
| `HydraApplication` | Root object; owns context, modules, config; drives the update loop |
| `EngineContext` | Type-safe service locator / DI container passed to every module |
| `ModuleManager` | Owns and drives all `IHydraModule` instances through lifecycle |
| `IHydraModule` | Interface every module must implement |
| `StartupSequence` | Ordered initialization steps |
| `ShutdownSequence` | Ordered teardown steps (reverse order) |
| `ApplicationSettings` | POD configuration struct with YAML loading |
| `ConfigManager` | YAML/JSON config loader with dot-path access |
| `Logger` | spdlog wrapper with macros |

### Module Lifecycle

```
OnRegister → OnInitialize → [OnUpdate / OnLateUpdate] × N → OnShutdown → OnUnregister
```

Shutdown always runs in reverse registration order.

---

## Dependencies

| Library | Purpose | Source |
|---|---|---|
| spdlog 1.15 | Logging | System (Nix) |
| nlohmann/json 3.11 | JSON + config | System (Nix) |
| yaml-cpp 0.8 | YAML config loading | System (Nix) |
| GoogleTest 1.16 | Unit testing | System (Nix) |
| GLM | Math (future modules) | System (Nix) |

**Zero dependency on:** Ogre3D, CUDA, OpenGL, Vulkan, DirectX, AI frameworks.

---

## Project Layout

```
HydraFlow/
├── CMakeLists.txt
├── config/
│   └── hydra.yaml              # Default config
├── engine/
│   └── HydraCore/
│       ├── include/HydraCore/
│       │   ├── HydraCore.h     # Umbrella header
│       │   ├── Common/         # Platform, Types, NonCopyable
│       │   ├── Application/    # HydraApplication, Modules, Context
│       │   ├── Logging/        # Logger + macros
│       │   └── Config/         # ConfigManager, ApplicationSettings
│       ├── src/                # Implementations
│       ├── tests/              # GoogleTest unit tests (29 tests)
│       ├── examples/
│       │   └── BasicApplication/main.cpp
│       └── CMakeLists.txt
├── scripts/
│   ├── build.sh
│   └── run_tests.sh
├── .clang-format
└── .clang-tidy
```

---

## Build

### Prerequisites (Nix / Linux)

```bash
nix-env -iA nixpkgs.cmake nixpkgs.ninja nixpkgs.gcc nixpkgs.spdlog \
            nixpkgs.nlohmann_json nixpkgs.yaml-cpp nixpkgs.gtest
```

### Configure and Build

```bash
cd HydraFlow
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build/Debug --parallel
```

Or use the helper script:

```bash
./scripts/build.sh Debug
./scripts/build.sh Release --clean
```

### Run Tests

```bash
./build/Debug/bin/HydraCore_Tests
# or
./scripts/run_tests.sh
```

### Run Example

```bash
./build/Debug/bin/HydraCore_BasicApp
```

---

## Quick Start (Module Usage)

```cpp
#include <HydraCore/HydraCore.h>

class MyModule : public Hydra::IHydraModule
{
public:
    Hydra::StringView GetName() const noexcept override { return "MyModule"; }

    bool OnInitialize(Hydra::EngineContext& ctx) override
    {
        HYDRA_LOG_INFO("[MyModule] Initialized");
        return true;
    }

    void OnUpdate(Hydra::EngineContext& ctx, Hydra::f64 dt) override
    {
        // simulate ...
    }
};

int main()
{
    Hydra::ApplicationSettings s = Hydra::ApplicationSettings::MakeDefault();
    s.appName = "My Simulation";

    Hydra::HydraApplication app(s);
    app.GetModules().Register<MyModule>();
    return app.Run();
}
```

---

## Design Principles

- SOLID, RAII, composition over inheritance
- No global state, no singletons (prefer DI via `EngineContext`)
- No raw `new`/`delete` — smart pointers throughout
- Macros only for platform abstraction, assertions, logging
- C++20 throughout
