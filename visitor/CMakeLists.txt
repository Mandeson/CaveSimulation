cmake_minimum_required(VERSION 3.16)

set(TARGET Visitor)
add_executable(${TARGET} "main.c"
                         "visitor.c")
target_link_libraries(${TARGET} common)
