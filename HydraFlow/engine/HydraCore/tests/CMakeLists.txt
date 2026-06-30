cmake_minimum_required(VERSION 3.22)

find_package(GTest REQUIRED)

add_executable(HydraCore_Tests
    # Module 1 tests
    test_Logger.cpp
    test_ConfigManager.cpp
    test_EngineContext.cpp
    test_ModuleManager.cpp
    test_HydraApplication.cpp

    # Module 2 tests
    test_Logging_Module2.cpp
)

target_link_libraries(HydraCore_Tests
    PRIVATE
        HydraFlow::HydraCore
        GTest::GTest
        GTest::Main
)

target_include_directories(HydraCore_Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(HydraCore_Tests)
