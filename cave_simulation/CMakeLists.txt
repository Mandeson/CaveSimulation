cmake_minimum_required(VERSION 3.16)

set(TARGET cave-simulation)
add_executable(${TARGET} "main.c"
                         "cave_simulation.c")
target_link_libraries(${TARGET} common)
