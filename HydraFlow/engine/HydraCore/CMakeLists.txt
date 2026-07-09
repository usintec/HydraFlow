cmake_minimum_required(VERSION 3.22)

# =============================================================================
# Dependencies
# =============================================================================

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(SPDLOG   REQUIRED spdlog)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

find_package(nlohmann_json REQUIRED)

find_package(glm QUIET)
if(NOT glm_FOUND)
    pkg_check_modules(GLM glm)
endif()

# =============================================================================
# HydraCore Static Library
# =============================================================================

add_library(HydraCore STATIC

    # ------------------------------------------------------------------
    # Module 1 — Application
    # Drives the engine lifecycle: startup, update loop, shutdown.
    # ------------------------------------------------------------------
    src/Application/HydraApplication.cpp
    src/Application/EngineContext.cpp
    src/Application/ModuleManager.cpp
    src/Application/StartupSequence.cpp
    src/Application/ShutdownSequence.cpp

    # ------------------------------------------------------------------
    # Module 1 — Config
    # YAML/JSON configuration loading and dot-path value access.
    # ------------------------------------------------------------------
    src/Config/ConfigManager.cpp
    src/Config/ApplicationSettings.cpp

    # ------------------------------------------------------------------
    # Module 2 — Logging
    # Full-featured logging: sinks, formatters, log rotation, macros.
    # ------------------------------------------------------------------
    src/Logging/LogLevel.cpp       # LogLevel enum utilities + ANSI colors
    src/Logging/ILogSink.cpp       # Abstract sink base — level filter + formatter
    src/Logging/Formatter.cpp      # PatternFormatter and JsonFormatter
    src/Logging/ConsoleSink.cpp    # Thread-safe colored console output
    src/Logging/FileSink.cpp       # Thread-safe rotating file output
    src/Logging/Logger.cpp         # Logger: sink dispatch, level guard, thread-safety
    src/Logging/LoggerFactory.cpp  # Global registry + convenience factory methods

    # ------------------------------------------------------------------
    # Module 3 — Configuration
    # Structured, typed, validated, environment-aware configuration.
    # ------------------------------------------------------------------
    src/Configuration/ConfigValue.cpp          # Dynamically-typed config tree node (nlohmann backend)
    src/Configuration/ConfigNode.cpp           # Path-aware cursor + dot-path navigation
    src/Configuration/Configuration.cpp        # Config document: root value + load/merge + YAML/JSON I/O
    src/Configuration/ConfigSchema.cpp         # Validation rules (RequireKey/Type/Range/AllowValues)
    src/Configuration/EnvironmentSettings.cpp  # Env enum + HYDRA_ENV detection
    src/Configuration/ConfigurationManager.cpp # Main façade: layered config, schema, hot reload

    # ------------------------------------------------------------------
    # Module 4 — Memory
    # Custom allocators, allocation tracking, leak detection, profiling hooks.
    # ------------------------------------------------------------------
    src/Memory/LinearAllocator.cpp     # Bump-pointer allocator
    src/Memory/StackAllocator.cpp      # LIFO allocator with marker rollback
    src/Memory/PoolAllocator.cpp       # Fixed-size block allocator
    src/Memory/FreeListAllocator.cpp   # Variable-size allocator with coalescing
    src/Memory/ArenaAllocator.cpp      # Paged bump-pointer allocator
    src/Memory/MemoryTracker.cpp       # Allocation tracking, statistics, hooks
    src/Memory/MemoryManager.cpp       # Global façade: tracked system alloc + registry

    # ------------------------------------------------------------------
    # Module 5 — Events
    # Typed pub/sub: synchronous dispatch, async queueing, priority order.
    # ------------------------------------------------------------------
    src/Events/EventDispatcher.cpp     # Type-keyed synchronous pub/sub hub
    src/Events/EventQueue.cpp          # Thread-safe FIFO for deferred dispatch
    src/Events/EventBus.cpp            # Façade combining dispatcher + queue

    # ------------------------------------------------------------------
    # Module 6 — Threading
    # ThreadPool/WorkerThread/Future/Promise, Mutex/CV/Semaphore wrappers,
    # thread utilities — the CPU-parallelism backbone of HydraCore.
    # ------------------------------------------------------------------
    src/Threading/ThreadUtils.cpp      # Thread naming, id, sleep/yield, hardware concurrency
    src/Threading/WorkerThread.cpp     # Named std::thread wrapper
    src/Threading/ThreadPool.cpp       # Worker pool + task queue + Submit()/WaitIdle()/Shutdown()

    # ------------------------------------------------------------------
    # Module 7 — Job System
    # Job/JobHandle, DependencyGraph, WorkerQueue, Task/TaskGraph, and the
    # JobScheduler that ties them together for parallel + dependency-aware
    # execution (with a hook reserved for future GPU job dispatch).
    # ------------------------------------------------------------------
    src/Jobs/DependencyGraph.cpp       # DAG of ready-counts/dependents keyed by JobId
    src/Jobs/WorkerQueue.cpp           # Thread-safe priority queue of ready Jobs
    src/Jobs/JobScheduler.cpp          # Worker pool + dependency cascade + TaskGraph submission
)

# =============================================================================
# Alias — used by subdirectories and external consumers
# =============================================================================

add_library(HydraFlow::HydraCore ALIAS HydraCore)

# =============================================================================
# Include Directories
# =============================================================================

target_include_directories(HydraCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SPDLOG_INCLUDE_DIRS}   # still needed for spdlog/fmt headers if used
        ${YAML_CPP_INCLUDE_DIRS}
)

# =============================================================================
# Link Libraries
# =============================================================================

target_link_libraries(HydraCore
    PUBLIC
        nlohmann_json::nlohmann_json
        Threads::Threads          # std::thread / pthread — required by Module 6 (Threading)
    PRIVATE
        ${YAML_CPP_LIBRARIES}
)

if(glm_FOUND)
    target_link_libraries(HydraCore PUBLIC glm::glm)
elseif(GLM_FOUND)
    target_include_directories(HydraCore PUBLIC ${GLM_INCLUDE_DIRS})
endif()

# =============================================================================
# Compile Definitions
# =============================================================================

target_compile_definitions(HydraCore
    PRIVATE
        HYDRACORE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        HYDRACORE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        HYDRACORE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    PUBLIC
        $<$<CONFIG:Debug>:HYDRA_DEBUG>
        $<$<CONFIG:Release>:HYDRA_RELEASE>
)

# =============================================================================
# Compile Features
# =============================================================================

target_compile_features(HydraCore PUBLIC cxx_std_20)

# =============================================================================
# Subdirectories
# =============================================================================

if(HYDRAFLOW_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(HYDRAFLOW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
