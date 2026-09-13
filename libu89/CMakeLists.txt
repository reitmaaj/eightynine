cmake_minimum_required(VERSION 3.20)
project(libu89 C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Source files under the green profile. This list is grown module by module as
# each becomes C89-and-C23 clean under both GCC and Clang (see green.yaml).
set(U89_SRCS
    src/u89_version.c
    src/u89_xid.c
    src/u89_id.c
    src/u89_props.c
    src/u89_width.c
    src/u89_utf8.c
    src/u89_grapheme.c
    src/u89_nfc.c
    src/u89_casefold.c
)

add_library(u89 STATIC ${U89_SRCS})
target_include_directories(u89 PUBLIC include PRIVATE src)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
