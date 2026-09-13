cmake_minimum_required(VERSION 3.20)
project(librepl89 C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Source files under the green profile. This list grows module by module as
# each becomes C89-and-C23 clean under both GCC and Clang (see green.yaml).
set(REPL89_SRCS
    src/repl89_mem.c
    src/repl89_buf.c
    src/repl89_edit.c
    src/repl89_hist.c
    src/repl89_render.c
    src/repl89_key.c
    src/repl89_paste.c
    src/repl89_tty.c
    src/repl89.c
)

add_library(repl89 STATIC ${REPL89_SRCS})
target_include_directories(repl89
    PUBLIC include
    PRIVATE src
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libu89/include")

# Example clients. The sibling libu89 archive must exist to build these
# targets; `just build` produces it first. CMake configuration only feeds the
# green gate's compile databases.
add_executable(echo examples/echo.c)
target_link_libraries(echo PRIVATE repl89
    "${CMAKE_CURRENT_SOURCE_DIR}/../libu89/build/libu89.a" util)

add_executable(echo_events examples/echo_events.c)
target_link_libraries(echo_events PRIVATE repl89
    "${CMAKE_CURRENT_SOURCE_DIR}/../libu89/build/libu89.a" util)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
