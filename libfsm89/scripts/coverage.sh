#!/bin/sh -eu
set -eu
# coverage.sh - require 100% line and branch coverage over src/.
#
# Builds the library and every fast suite with gcov instrumentation, runs
# them, and fails if any source line or branch was never executed.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/cov"
cc="${COV_CC:-gcc}"

rm -rf "$out"
mkdir -p "$out"

set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -O0 --coverage
set -- "$@" -I"$root/include" -I"$root/src" -I"$root/test/support"
set -- "$@" -I"$root/test/model"

for f in "$root"/src/*.c; do
    name=$(basename "$f" .c)
    "$cc" "$@" -c "$f" -o "$out/$name.o"
done
ar rcs "$out/libfsm89.a" "$out"/*.o

for t in "$root"/test/smoke.c "$root"/test/unit/test_*.c \
    "$root"/test/model/test_*.c "$root"/test/generated/test_*.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$cc" "$@" -Wno-unused-function -o "$out/$name" "$t" \
        "$root/test/model/ref_fsm89.c" "$root/test/support/fsm89_gen.c" \
        "$root/test/support/fsm89_test.c" \
        "$root/test/support/fsm89_fixtures.c" "$out/libfsm89.a"
    "$out/$name" > /dev/null
done

cd "$out"
for gcno in fsm89_*.gcno; do
    [ -e "$gcno" ] || continue
    gcov -b -c "$gcno" > /dev/null 2>&1
done

sh "$root/scripts/coverage-check.sh" "$root/src" "$out"
