cmake_minimum_required(VERSION 3.20)
project(libcksum89 C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CKSUM89_SRCS
    src/cksum89_tables.c)

add_library(cksum89 STATIC ${CKSUM89_SRCS})
add_library(cksum89::cksum89 ALIAS cksum89)
target_include_directories(cksum89
    PUBLIC include
    PRIVATE src)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CKSUM89_TESTS_DEFAULT ON)
else()
    set(CKSUM89_TESTS_DEFAULT OFF)
endif()
option(CKSUM89_BUILD_TESTS "Build the libcksum89 test suite"
       ${CKSUM89_TESTS_DEFAULT})

if(CKSUM89_BUILD_TESTS)
    enable_testing()
    add_executable(test_cksum89
        test/test_main.c
        test/ref_crc.c
        test/test_vectors.c
        test/test_tables.c)
    target_include_directories(test_cksum89 PRIVATE include src test)
    target_link_libraries(test_cksum89 PRIVATE cksum89)
    add_test(NAME cksum89 COMMAND test_cksum89)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
