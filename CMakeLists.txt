cmake_minimum_required(VERSION 3.7)
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(choose)
add_executable(choose choose.cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(choose PRIVATE -Wall -Wextra -Wpedantic -Os -fno-rtti -fno-strict-aliasing)
endif()

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
target_include_directories(choose PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(choose PRIVATE ${CURSES_LIBRARIES})
target_compile_options(choose PRIVATE ${CURSES_CFLAGS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE REQUIRED libpcre2-8)

target_include_directories(choose PRIVATE ${PCRE_INCLUDEDIR})
target_link_libraries(choose PRIVATE ${PCRE_LIBRARIES}) 

install(TARGETS choose DESTINATION /usr/bin)