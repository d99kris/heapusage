# Project
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(heapusage VERSION 1.0 LANGUAGES C CXX)

set(COMMON_FLAGS "-funwind-tables -g -Wall -Wextra -Wpedantic -Wshadow \
                  -Wpointer-arith -Wcast-qual -Wno-missing-braces \
                  -Wswitch-default -Wcast-align -Wunreachable-code \
                  -Wuninitialized")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(TOOLCHAIN_FLAGS "")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(TOOLCHAIN_FLAGS "-Wno-stringop-overflow")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_FLAGS} ${TOOLCHAIN_FLAGS} \
                   -Wstrict-prototypes -Wmissing-prototypes")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} ${TOOLCHAIN_FLAGS}")

# C++ compiler flags
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Library
add_library(heapusage SHARED src/humain.cpp src/hulog.cpp src/humalloc.cpp src/huapi.cpp)
set_target_properties(heapusage PROPERTIES PUBLIC_HEADER "src/huapi.h")
target_compile_features(heapusage PRIVATE cxx_variadic_templates)
install(TARGETS heapusage
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
target_link_libraries(heapusage pthread dl)

# Dependency backward-cpp providing more detailed stacktraces on Linux, when
# either of the following is present (only ONE needed):
#   libbfd    - sudo apt install binutils-dev
#   libdw     - sudo apt install libdw-dev
#   libdwarf  - sudo apt install libdwarf-dev
add_subdirectory(ext/backward-cpp)
add_backward(heapusage)

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
