cmake_minimum_required(VERSION 3.8)

project(test)
set (CMAKE_CXX_STANDARD 17)

include_directories(inc)

add_executable(${CMAKE_PROJECT_NAME} src/test.c)
add_executable(recv_loop src/recv_loop.cpp src/axis_fifo.cpp)
target_compile_options(recv_loop PRIVATE -Wall -Wextra -pedantic)

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION bin)