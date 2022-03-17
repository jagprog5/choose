cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
project(choose)

add_executable(choose choose.cpp)
target_compile_options(choose PRIVATE -Wall -Wextra -Wpedantic -O1)

target_link_libraries(choose PRIVATE ncurses)

install(TARGETS choose)