cmake_minimum_required(VERSION 3.10)
project(x87test)

set (CMAKE_CXX_STANDARD 17)

add_subdirectory(fmt)

add_executable(x87test main.cpp soft_x87.cpp)
target_link_libraries(x87test fmt::fmt)
