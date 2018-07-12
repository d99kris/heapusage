# Project
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(heapusage VERSION 1.0 LANGUAGES C CXX)
set(COMMON_FLAGS "-funwind-tables -g -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual \
                  -Wno-missing-braces -Wswitch-default -Wcast-align -Wunreachable-code -Wundef \
                  -Wuninitialized")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_FLAGS} -Wstrict-prototypes -Wmissing-prototypes")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${COMMON_FLAGS}")

# Library
add_library(heapusage SHARED src/humain.c src/hulog.cpp)
target_compile_features(heapusage PRIVATE cxx_variadic_templates)
install(TARGETS heapusage LIBRARY DESTINATION lib)
set_target_properties(heapusage PROPERTIES LINK_FLAGS "-fPIC")
target_link_libraries(heapusage pthread dl)

# Utility
configure_file(src/heapusage ${CMAKE_CURRENT_BINARY_DIR}/heapusage COPYONLY)
install(PROGRAMS src/heapusage DESTINATION bin)

# Manual
install(FILES src/heapusage.1 DESTINATION share/man/man1)

# Tests
enable_testing()

add_executable(ex001 tests/ex001.c)
add_executable(ex002 tests/ex002.c)

configure_file(tests/test001 ${CMAKE_CURRENT_BINARY_DIR}/test001 COPYONLY)
add_test(test001 "${PROJECT_BINARY_DIR}/test001")

configure_file(tests/test002 ${CMAKE_CURRENT_BINARY_DIR}/test002 COPYONLY)
add_test(test002 "${PROJECT_BINARY_DIR}/test002")

