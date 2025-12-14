cmake_minimum_required(VERSION 3.16)

project(CaveSimulation)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

add_subdirectory(common)

set(TARGET cave_simulation)
add_executable(${TARGET} "cave_simulation.c")
target_link_libraries(${TARGET} common)
