#!/bin/sh -eu
# sanitize.sh - rebuild libstr89 and the fast suite with ASan + UBSan, then
# run every test binary under the sanitizers.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
U89=${U89:-"$ROOT/../libu89"}
OUT="$ROOT/build/asan"
CC=${ASAN_CC:-clang}

mkdir -p "$OUT"

set -- -std=c99 -pedantic-errors -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g

rm -f "$OUT"/*.o "$OUT"/libstr89.a
for f in "$ROOT"/src/*.c; do
    name=$(basename "$f" .c)
    "$CC" "$@" -I"$ROOT/include" -I"$U89/include" -c "$f" -o "$OUT/$name.o"
done
ar rcs "$OUT/libstr89.a" "$OUT"/*.o

for t in "$ROOT"/test/smoke.c "$ROOT"/test/unit/test_*.c \
    "$ROOT"/test/fault/test_*.c "$ROOT"/test/model/test_*.c \
    "$ROOT"/test/corpus/test_*.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$CC" "$@" -Wno-unused-function -I"$ROOT/include" -I"$U89/include" \
        -I"$ROOT/src" -I"$ROOT/test/support" -o "$OUT/$name" "$t" \
        "$ROOT/test/support/str89_test.c" "$OUT/libstr89.a" \
        "$U89/build/libu89.a"
    ASAN_OPTIONS=detect_leaks=1 "$OUT/$name"
done
echo "sanitize: ok"
