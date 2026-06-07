cmake_minimum_required(VERSION 3.16)
project(OrderBookMatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build: full optimisation + architecture-specific tuning
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Collect all translation units
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/order/*.cpp"
    "${CMAKE_SOURCE_DIR}/book/*.cpp"
    "${CMAKE_SOURCE_DIR}/feed/*.cpp"
    "${CMAKE_SOURCE_DIR}/mem/*.cpp"
    "${CMAKE_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_SOURCE_DIR}/main.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Allow #include "order/Order.hpp", "book/OrderBook.hpp", etc. from anywhere.
# target_include_directories MUST come after add_executable.
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

# Optimisation flags (GCC/Clang)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:Debug>:-g -O0 -fsanitize=address,undefined>
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
