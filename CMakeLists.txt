cmake_minimum_required(VERSION 3.16)

project(CaveSimulation)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

add_subdirectory(common)
add_subdirectory(cave_simulation)
add_subdirectory(ticket_clerk)
add_subdirectory(visitor)
add_subdirectory(guide)
add_subdirectory(guard)
