cmake_minimum_required(VERSION 3.22)

# =============================================================================
# Dependencies
# =============================================================================

find_package(PkgConfig REQUIRED)

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
