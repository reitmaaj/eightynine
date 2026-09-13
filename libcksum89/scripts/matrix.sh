#!/bin/sh -eu
set -eu
# matrix.sh - build libcksum89 and run the unit suite under gcc and clang in
# both strict C89 and strict C23 modes.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/build/matrix"

mkdir -p "$OUT"

set -- -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
    -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
    -Wundef -Wshadow -Wformat=2

for compiler in gcc clang; do
    for std in c89 c23; do
        echo "matrix: $compiler -std=$std"
        rm -f "$OUT"/*.o "$OUT"/test_cksum89 "$OUT"/smoke
        for f in "$ROOT"/src/*.c; do
            [ -e "$f" ] || continue
            name=$(basename "$f" .c)
            "$compiler" -std="$std" "$@" \
                -I"$ROOT/include" -I"$ROOT/src" -c "$f" -o "$OUT/lib_$name.o"
        done
        for f in "$ROOT"/test/*.c; do
            [ -e "$f" ] || continue
            case "$f" in
                */smoke.c) continue ;;
            esac
            name=$(basename "$f" .c)
            "$compiler" -std="$std" "$@" -Wno-unused-function \
                -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/test" \
                -c "$f" -o "$OUT/test_$name.o"
        done
        "$compiler" -std="$std" "$@" -Wno-unused-function \
            -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/test" \
            "$OUT"/lib_*.o "$OUT"/test_*.o -o "$OUT/test_cksum89"
        "$OUT/test_cksum89"
        if [ -e "$ROOT/test/smoke.c" ]; then
            "$compiler" -std="$std" "$@" -Wno-unused-function \
                -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/test" \
                "$ROOT/test/smoke.c" "$OUT"/lib_*.o -o "$OUT/smoke"
            "$OUT/smoke"
        fi
    done
done
echo "matrix: ok"
