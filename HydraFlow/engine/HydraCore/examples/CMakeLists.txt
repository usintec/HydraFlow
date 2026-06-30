cmake_minimum_required(VERSION 3.22)

add_executable(HydraCore_BasicApp
    BasicApplication/main.cpp
)

target_link_libraries(HydraCore_BasicApp
    PRIVATE
        HydraFlow::HydraCore
)
