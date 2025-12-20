cmake_minimum_required(VERSION 3.16)

project(CaveSimulation)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

add_subdirectory(common)
add_subdirectory(cave_simulation)
add_subdirectory(ticket_clerk)
