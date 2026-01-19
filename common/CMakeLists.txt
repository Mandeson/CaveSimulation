cmake_minimum_required(VERSION 3.16)

set(TARGET_COMMON common)
add_library(${TARGET_COMMON} "common.c"
                             "logger_interface.c"
                             "util/array.c"
                             "util/time.c")
target_include_directories(${TARGET_COMMON} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")
