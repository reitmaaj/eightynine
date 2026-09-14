#!/bin/sh -eu
set -eu
# matrix.sh - compile and run the behavioral suite under gcc and clang in both
# strict C89 and strict C23 modes, then under -O0/-O2/-O3.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/matrix"

mkdir -p "$out"

set -- -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion
set -- "$@" -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition
set -- "$@" -Wundef -Wshadow -Wformat=2

run_cell() {
    compiler="$1"
    std="$2"
    opt="$3"
    shift 3

    echo "matrix: $compiler -std=$std $opt"
    rm -f "$out"/*.o "$out"/libsyntax89.a
    for f in "$root"/src/*.c; do
        name=$(basename "$f" .c)
        "$compiler" -std="$std" "$@" "$opt" -I"$root/include" -c "$f" \
            -o "$out/$name.o"
    done
    ar rcs "$out/libsyntax89.a" "$out"/*.o
    for t in "$root"/test/smoke.c "$root"/test/unit/test_*.c \
        "$root"/test/model/test_*.c "$root"/test/generated/test_*.c \
        "$root"/test/fault/test_*.c "$root"/test/stress/test_*.c; do
        [ -e "$t" ] || continue
        name=$(basename "$t" .c)
        "$compiler" -std="$std" "$@" "$opt" -Wno-unused-function \
            -I"$root/include" -I"$root/src" -I"$root/test/support" \
            -I"$root/test/model" -o "$out/$name" "$t" \
            "$root/test/model/ref_syntax89.c" \
            "$root/test/support/syntax89_gen.c" \
            "$root/test/support/syntax89_test.c" \
            "$root/test/support/syntax89_fixtures.c" \
            "$root/test/support/syntax89_invariants.c" \
            "$root/test/support/syntax89_fault_alloc.c" "$out/libsyntax89.a"
        "$out/$name" > /dev/null
    done
}

for compiler in gcc clang; do
    for std in c89 c23; do
        run_cell "$compiler" "$std" -O0 "$@"
    done
done

for opt in -O0 -O2 -O3; do
    run_cell gcc c89 "$opt" "$@"
done

echo "matrix: ok"
