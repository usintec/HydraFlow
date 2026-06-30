cmake_minimum_required(VERSION 3.22)

project(HydraFlow
    VERSION 0.1.0
    DESCRIPTION "HydraFlow Scientific Computing and Intelligent Graphics Platform"
    LANGUAGES CXX
)

# ==============================================================================
# Global C++ Standard
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Build Type
# ==============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ==============================================================================
# Output Directories
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ==============================================================================
# Compiler Options
# ==============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/DNDEBUG>
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ==============================================================================
# CMake Modules
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ==============================================================================
# Options
# ==============================================================================
option(HYDRAFLOW_BUILD_TESTS     "Build unit tests"    ON)
option(HYDRAFLOW_BUILD_EXAMPLES  "Build examples"      ON)
option(HYDRAFLOW_BUILD_BENCHMARKS "Build benchmarks"   OFF)
option(HYDRAFLOW_BUILD_DOCS      "Build documentation" OFF)

# ==============================================================================
# Submodules
# ==============================================================================
add_subdirectory(engine/HydraCore)
