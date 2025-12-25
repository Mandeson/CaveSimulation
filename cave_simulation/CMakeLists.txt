cmake_minimum_required(VERSION 3.16)

set(TARGET CaveSimulation)
add_executable(${TARGET} "main.c"
                         "cave_simulation.c"
                         "operations.c")
target_link_libraries(${TARGET} common)
