cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
project(choose)

add_executable(choose choose.cpp)
target_compile_options(choose PRIVATE -Wall -Wextra -Wpedantic -O1)

target_link_libraries(choose PRIVATE ncurses)


# target_include_directories(${PROJECT_NAME}_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# if(BUILD_TESTING)
#   # fuzzer
#   add_executable(fuzz_sdr src/fuzz.cpp)
#   target_compile_options(fuzz_sdr PRIVATE -Wall -Wextra -Wpedantic -Ofast)
#   target_link_libraries(fuzz_sdr PRIVATE ${PROJECT_NAME}_lib)
  
#   # unit tests
#   set(BOOST_MIN_VERSION "1.63.0")
#   add_executable(test_sdr src/test.cpp)
#   target_compile_options(fuzz_sdr PRIVATE -Wall -Wextra -Wpedantic -Ofast)
#   find_package(Boost REQUIRED COMPONENTS unit_test_framework)
#   target_link_libraries(test_sdr PRIVATE ${PROJECT_NAME}_lib ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
  
# endif()