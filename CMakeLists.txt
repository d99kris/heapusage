# Project
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(heapusage VERSION 1.0 LANGUAGES C CXX)
set(COMMON_FLAGS "-funwind-tables -g -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual \
                  -Wno-missing-braces -Wswitch-default -Wcast-align -Wunreachable-code -Wundef \
                  -Wuninitialized")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_FLAGS} -Wstrict-prototypes \
                   -Wmissing-prototypes")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

# C++ compiler flags
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optional dependency - binutils / binutils-dev - for filename/line-numbers in callstacks
include(FindPackageHandleStandardArgs)
find_path(BFD_INCLUDE_PATH bfd.h PATH /usr/include /usr/local/include /usr/local/opt/*/include)
find_library(BFD_LIBRARIES bfd PATH /lib /usr/lib /usr/local/lib /usr/local/opt/*/lib)
find_package_handle_standard_args(BFD DEFAULT_MSG BFD_LIBRARIES BFD_INCLUDE_PATH)
if(BFD_FOUND)
  set(EXTRA_LIBRARIES "${BFD_LIBRARIES}")
  include_directories("ext/backward-cpp" ${BFD_INCLUDE_DIR})
  add_definitions(-DHAS_BFD)
endif()

# Library
add_library(heapusage SHARED src/humain.cpp src/hulog.cpp src/humalloc.cpp)
target_compile_features(heapusage PRIVATE cxx_variadic_templates)
install(TARGETS heapusage LIBRARY DESTINATION lib)
target_link_libraries(heapusage ${EXTRA_LIBRARIES} pthread dl)

# Utility
configure_file(src/heapusage ${CMAKE_CURRENT_BINARY_DIR}/heapusage COPYONLY)
install(PROGRAMS src/heapusage DESTINATION bin)

# Manual
install(FILES src/heapusage.1 DESTINATION share/man/man1)

# Tests
enable_testing()

add_executable(ex001 tests/ex001.c)
add_executable(ex002 tests/ex002.c)
add_executable(ex003 tests/ex003.cpp)
add_executable(ex004 tests/ex004.cpp)
add_executable(ex005 tests/ex005.cpp)

configure_file(tests/test001 ${CMAKE_CURRENT_BINARY_DIR}/test001 COPYONLY)
add_test(test001 "${PROJECT_BINARY_DIR}/test001")

configure_file(tests/test002 ${CMAKE_CURRENT_BINARY_DIR}/test002 COPYONLY)
add_test(test002 "${PROJECT_BINARY_DIR}/test002")

configure_file(tests/test003 ${CMAKE_CURRENT_BINARY_DIR}/test003 COPYONLY)
add_test(test003 "${PROJECT_BINARY_DIR}/test003")

configure_file(tests/test004 ${CMAKE_CURRENT_BINARY_DIR}/test004 COPYONLY)
add_test(test004 "${PROJECT_BINARY_DIR}/test004")

configure_file(tests/test005 ${CMAKE_CURRENT_BINARY_DIR}/test005 COPYONLY)
add_test(test005 "${PROJECT_BINARY_DIR}/test005")
