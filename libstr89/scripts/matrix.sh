#!/bin/sh -eu
# matrix.sh - compile and run the behavioral suite under gcc and clang in both
# strict C89 and strict C23 modes.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
U89=${U89:-"$ROOT/../libu89"}
OUT="$ROOT/build/matrix"

mkdir -p "$OUT"

set -- -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
    -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
    -Wundef -Wshadow -Wformat=2

for compiler in gcc clang; do
    for std in c89 c23; do
        echo "matrix: $compiler -std=$std"
        rm -f "$OUT"/*.o "$OUT"/libstr89.a
        for f in "$ROOT"/src/*.c; do
            name=$(basename "$f" .c)
            "$compiler" -std="$std" "$@" \
                -I"$ROOT/include" -I"$U89/include" -c "$f" -o "$OUT/$name.o"
        done
        ar rcs "$OUT/libstr89.a" "$OUT"/*.o
        for t in "$ROOT"/test/unit/test_*.c "$ROOT"/test/fault/test_*.c \
            "$ROOT"/test/model/test_*.c "$ROOT"/test/corpus/test_*.c; do
            [ -e "$t" ] || continue
            name=$(basename "$t" .c)
            "$compiler" -std="$std" "$@" -Wno-unused-function \
                -I"$ROOT/include" -I"$U89/include" -I"$ROOT/src" \
                -I"$ROOT/test/support" -o "$OUT/$name" "$t" \
                "$ROOT/test/support/str89_test.c" "$OUT/libstr89.a" \
                "$U89/build/libu89.a"
            "$OUT/$name"
        done
    done
done
echo "matrix: ok"
