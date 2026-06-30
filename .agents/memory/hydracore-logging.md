---
name: HydraCore Logging Module 2
description: Architecture decisions and traps for the Module 2 logging system.
---

# HydraCore — Logging Module 2 Architecture

## Key Design Decisions

**Logger is instance-based, not static.**
Module 1 had a thin static spdlog wrapper. Module 2 replaces it with a proper
instance-based `Logger` class owned by `LoggerFactory`. Any subsystem that used
`Logger::Initialize()` / `Logger::Shutdown()` must now use `LoggerFactory`.

**Why:** Static loggers can't be scoped per subsystem; tests can't isolate them.

## Critical Deadlock Trap: LoggerFactory::Initialize()

`LoggerFactory::Initialize()` MUST release `s_Mutex` before calling `Create()`,
because `Create()` also acquires `s_Mutex`. Pattern:

```cpp
{ std::lock_guard lock(s_Mutex); if (s_Initialized) return; s_Initialized = true; }
auto logger = Create(config);   // lock NOT held here
{ std::lock_guard lock(s_Mutex); s_Default = logger; }
```

**Why:** Any inline call to `Create()` / `BuildLogger()` while holding `s_Mutex`
will deadlock. This bit us during testing (test hung forever at LoggerFactory.InitializedAfterInit).

## HYDRA_DEBUG Macro Conflict

Do NOT define `HYDRA_DEBUG(...)` as a logging macro. CMake emits `-DHYDRA_DEBUG`
as a compile flag in Debug builds (from `$<$<CONFIG:Debug>:HYDRA_DEBUG>`), causing
a redefinition warning or error. Use `HYDRA_LOG_DEBUG(...)` for debug logging.

**How to apply:** If a future module wants a `HYDRA_*` alias at Debug level,
name it `HYDRA_DBG` or similar to avoid the CMake flag collision.

## std::format on GCC 14 / C++20

`std::format` is available and works correctly. Testing with `#include <format>`
and no `main()` will give a linker error (not a compilation error) — this is
expected and does NOT mean std::format is unavailable.

## Test Fixture Name Collision

Each test file that uses a `LoggerTest` fixture conflicts with the other.
The Module 2 test file uses `LoggerInstanceTest` to avoid collision with
Module 1's `test_Logger.cpp`.

## Module Lifetime vs. Test Assertions

After `HydraApplication::Run()` returns, all modules are destroyed by
`ShutdownAll()`. Any reference obtained via `Register<T>(...)` is dangling.
Pattern: have the module write its final state to an external variable in
`OnShutdown()`, then read that variable after `Run()`.
