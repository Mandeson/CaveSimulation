cmake_minimum_required(VERSION 3.16)

set(TARGET_COMMON common)
add_library(${TARGET_COMMON} INTERFACE)
target_include_directories(${TARGET_COMMON} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/")
