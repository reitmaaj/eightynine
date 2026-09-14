#!/bin/sh -eu
set -eu
# valgrind.sh - run the fast suite under valgrind memcheck when available.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/valgrind"
cc="${CC:-cc}"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind: UNSUPPORTED (valgrind not on PATH)"
    exit 0
fi

mkdir -p "$out"

set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion
set -- "$@" -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes
set -- "$@" -Wold-style-definition -Wundef -Wshadow -Wformat=2

for f in "$root"/src/*.c; do
    name=$(basename "$f" .c)
    "$cc" "$@" -I"$root/include" -c "$f" -o "$out/$name.o"
done
ar rcs "$out/libsyntax89.a" "$out"/*.o

for t in "$root"/test/smoke.c "$root"/test/unit/test_*.c \
    "$root"/test/fault/test_*.c "$root"/test/model/test_*.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$cc" "$@" -Wno-unused-function -I"$root/include" -I"$root/src" \
        -I"$root/test/support" -I"$root/test/model" -o "$out/$name" "$t" \
        "$root/test/model/ref_syntax89.c" \
        "$root/test/support/syntax89_gen.c" \
        "$root/test/support/syntax89_test.c" \
        "$root/test/support/syntax89_fixtures.c" \
        "$root/test/support/syntax89_invariants.c" \
        "$root/test/support/syntax89_fault_alloc.c" "$out/libsyntax89.a"
    valgrind --error-exitcode=1 --leak-check=full --quiet "$out/$name"
done

echo "valgrind: ok"
