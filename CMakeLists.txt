cmake_minimum_required(VERSION 3.7)
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(NO_SCROLL_BORDER "Disable the scroll border" OFF)
option(DISABLE_FIELD "Disable --field and slightly increases the speed of sorting and uniqueness" OFF)
option(BUILD_TESTING "Build tests" OFF)
option(CODE_COVERAGE "Create coverage stats. Requires BUILD_TESTING. Invoke with make cov-show" OFF)
project(choose)
add_executable(choose src/main.cpp)
target_include_directories(choose PRIVATE src)

target_compile_options(choose PRIVATE -Wall -Wextra -O3)

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

# https://stackoverflow.com/a/74755391/15534181
# optional link here since sometimes it's ok not to have. this is the least invasive
find_package(TBB QUIET)
if(TBB_FOUND)
  target_link_libraries(choose PRIVATE TBB::tbb)
endif()

if (NO_SCROLL_BORDER)
  target_compile_definitions(choose PRIVATE
    CHOOSE_NO_SCROLL_BORDER
  )
endif()

if (DISABLE_FIELD)
  target_compile_definitions(choose PRIVATE
    CHOOSE_DISABLE_FIELD
  )
endif()

install(TARGETS choose DESTINATION bin)

# this should only be invoked by the uninstall script, and not manually
add_custom_target(remove_exe
    rm -vf -- "${CMAKE_INSTALL_PREFIX}/bin/choose"
)

if(BUILD_TESTING)
  set(BOOST_MIN_VERSION "1.63.0")
  # valgrind indicates mem leak within boost when using shared lib
  set(Boost_USE_STATIC_LIBS ON)
  find_package(Boost REQUIRED COMPONENTS unit_test_framework)
  
  add_executable(unit_tests src/test.cpp)
  target_include_directories(unit_tests PRIVATE src)
  # -O0 gives better coverage info than -Og
  target_compile_options(unit_tests PRIVATE -Wall -Wextra -O0 -g)

  target_include_directories(unit_tests PRIVATE ${Boost_UNIT_TEST_FRAMEWORK_HEADER_NAME})
  target_link_libraries(unit_tests PRIVATE ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

  target_include_directories(unit_tests PRIVATE ${CURSES_INCLUDE_DIRS})
  target_link_libraries(unit_tests PRIVATE ${CURSES_LIBRARIES})
  target_compile_options(unit_tests PRIVATE ${CURSES_CFLAGS})

  target_include_directories(unit_tests PRIVATE ${PCRE_INCLUDEDIR})
  target_link_libraries(unit_tests PRIVATE ${PCRE_LIBRARIES})

  if(TBB_FOUND)
    target_link_libraries(unit_tests PRIVATE TBB::tbb)
  endif()

  if (DISABLE_FIELD)
    target_compile_definitions(unit_tests PRIVATE
      CHOOSE_DISABLE_FIELD
    )
  endif()

  enable_testing()
  add_test(COMMAND ./unit_tests)

  if (CODE_COVERAGE)
    target_compile_options(unit_tests PRIVATE --coverage)
    target_link_libraries(unit_tests PRIVATE --coverage)

    add_custom_target(cov-clean
      COMMAND find "${CMAKE_CURRENT_BINARY_DIR}" -type f -name '*.gcda' -delete
      COMMAND rm -f "${CMAKE_CURRENT_BINARY_DIR}/coverage.info"
      COMMAND rm -f "${CMAKE_CURRENT_BINARY_DIR}/*.png"
      COMMAND find "${CMAKE_CURRENT_BINARY_DIR}" -type f -name '*.html' -delete
      COMMAND rm -f "${CMAKE_CURRENT_BINARY_DIR}/gcov.css"
    )

    # used to make cov-proc run the tests if they haven't yet been run
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/unit_tests.dir/src/test.cpp.gcda
      DEPENDS unit_tests
      COMMAND ctest -j "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_custom_target(cov-proc
      # the tests must be run before the coverage can be processed
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/unit_tests.dir/src/test.cpp.gcda
      COMMAND lcov --capture --directory "${CMAKE_CURRENT_SOURCE_DIR}" --base-directory "${CMAKE_CURRENT_SOURCE_DIR}/src" --no-external --output-file "${CMAKE_CURRENT_BINARY_DIR}/coverage.info"
    )

    add_custom_target(cov-show
      DEPENDS cov-proc
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND genhtml coverage.info
      COMMAND sensible-browser index.html
    )
  endif()
endif()
