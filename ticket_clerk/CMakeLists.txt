cmake_minimum_required(VERSION 3.16)

set(TARGET TicketClerk)
add_executable(${TARGET} "main.c")
target_link_libraries(${TARGET} common)
