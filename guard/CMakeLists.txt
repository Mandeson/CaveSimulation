cmake_minimum_required(VERSION 3.16)

set(TARGET Guard)
add_executable(${TARGET} "main.c")
target_link_libraries(${TARGET} common)
