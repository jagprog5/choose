cmake_minimum_required(VERSION 3.7)
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

OPTION(NO_SCROLL_BORDER "Disable the scroll border" OFF)
option(BUILD_TESTING "Build tests" OFF)
project(choose)
add_executable(choose src/main.cpp)
target_include_directories(choose PRIVATE src)

target_compile_options(choose PRIVATE -Wall -Wextra -Wpedantic -Os -fno-rtti -fno-strict-aliasing)

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

if (NO_SCROLL_BORDER)
  target_compile_definitions(choose PRIVATE
    CHOOSE_NO_SCROLL_BORDER
  )
endif()

install(TARGETS choose DESTINATION /usr/bin)

# hist

add_custom_command(
  TARGET choose
  POST_BUILD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND scripts/hist_installer.bash
)

if(BUILD_TESTING)
  set(BOOST_MIN_VERSION "1.63.0")
  # valgrind indicates mem leak within boost when using shared lib
  set(Boost_USE_STATIC_LIBS ON)
  find_package(Boost REQUIRED COMPONENTS unit_test_framework)
  add_executable(test src/test.cpp)
  target_include_directories(test PRIVATE src)
  target_compile_options(test PRIVATE -Wall -Wextra -O0 -g)
  target_link_libraries(test PRIVATE ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

  target_include_directories(test PRIVATE ${CURSES_INCLUDE_DIRS})
  target_link_libraries(test PRIVATE ${CURSES_LIBRARIES})
  target_compile_options(test PRIVATE ${CURSES_CFLAGS})

  target_include_directories(test PRIVATE ${PCRE_INCLUDEDIR})
  target_link_libraries(test PRIVATE ${PCRE_LIBRARIES})

endif()