#!/bin/sh -eu
# valgrind.sh - run the built fast-suite binaries under valgrind memcheck.
# Degrades to a notice when valgrind is unavailable.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
U89=${U89:-"$ROOT/../libu89"}
OUT="$ROOT/build"
CC=${CC:-cc}

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind: UNSUPPORTED (valgrind not on PATH)"
    exit 0
fi

mkdir -p "$OUT"
set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion \
    -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes \
    -Wold-style-definition -Wundef -Wshadow -Wformat=2

for t in "$ROOT"/test/smoke.c "$ROOT"/test/unit/test_*.c \
    "$ROOT"/test/fault/test_*.c "$ROOT"/test/model/test_*.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$CC" "$@" -Wno-unused-function -I"$ROOT/include" -I"$U89/include" \
        -I"$ROOT/src" -I"$ROOT/test/support" -o "$OUT/$name" "$t" \
        "$ROOT/test/support/str89_test.c" "$OUT/libstr89.a" \
        "$U89/build/libu89.a"
    valgrind --error-exitcode=1 --leak-check=full --quiet "$OUT/$name"
done
echo "valgrind: ok"
