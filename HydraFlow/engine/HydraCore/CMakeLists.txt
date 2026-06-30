cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Find Dependencies
# ==============================================================================
find_package(PkgConfig REQUIRED)

pkg_check_modules(SPDLOG REQUIRED spdlog)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

find_package(nlohmann_json REQUIRED)

# Try to find GLM via pkg-config or cmake config
find_package(glm QUIET)
if(NOT glm_FOUND)
    pkg_check_modules(GLM glm)
endif()

# ==============================================================================
# HydraCore Library Target
# ==============================================================================
add_library(HydraCore STATIC
    # Application
    src/Application/HydraApplication.cpp
    src/Application/EngineContext.cpp
    src/Application/ModuleManager.cpp
    src/Application/StartupSequence.cpp
    src/Application/ShutdownSequence.cpp

    # Logging
    src/Logging/Logger.cpp
    src/Logging/LogSink.cpp

    # Config
    src/Config/ConfigManager.cpp
    src/Config/ApplicationSettings.cpp
)

# ==============================================================================
# Alias for clean usage in subdirs
# ==============================================================================
add_library(HydraFlow::HydraCore ALIAS HydraCore)

# ==============================================================================
# Include Directories
# ==============================================================================
target_include_directories(HydraCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SPDLOG_INCLUDE_DIRS}
        ${YAML_CPP_INCLUDE_DIRS}
)

# ==============================================================================
# Link Libraries
# ==============================================================================
target_link_libraries(HydraCore
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        ${SPDLOG_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
)

# Link GLM if found
if(glm_FOUND)
    target_link_libraries(HydraCore PUBLIC glm::glm)
elseif(GLM_FOUND)
    target_include_directories(HydraCore PUBLIC ${GLM_INCLUDE_DIRS})
endif()

# ==============================================================================
# Compile Definitions
# ==============================================================================
target_compile_definitions(HydraCore
    PRIVATE
        HYDRACORE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        HYDRACORE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        HYDRACORE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    PUBLIC
        $<$<CONFIG:Debug>:HYDRA_DEBUG>
        $<$<CONFIG:Release>:HYDRA_RELEASE>
)

# ==============================================================================
# Compile Features
# ==============================================================================
target_compile_features(HydraCore PUBLIC cxx_std_20)

# ==============================================================================
# Tests
# ==============================================================================
if(HYDRAFLOW_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ==============================================================================
# Examples
# ==============================================================================
if(HYDRAFLOW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
