#!/bin/sh -eu
# sanitize.sh - rebuild libfsm89 and the fast suite with ASan + UBSan, then
# run every test binary under the sanitizers.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/asan"
cc="${ASAN_CC:-clang}"

mkdir -p "$out"

set -- -std=c99 -pedantic-errors -Wall -Wextra -Werror
set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer -g

rm -f "$out"/*.o "$out"/libfsm89.a
for f in "$root"/src/*.c; do
    name=$(basename "$f" .c)
    "$cc" "$@" -I"$root/include" -c "$f" -o "$out/$name.o"
done
ar rcs "$out/libfsm89.a" "$out"/*.o

for t in "$root"/test/smoke.c "$root"/test/unit/test_*.c \
    "$root"/test/model/test_*.c "$root"/test/generated/test_*.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$cc" "$@" -Wno-unused-function -I"$root/include" -I"$root/src" \
        -I"$root/test/support" -I"$root/test/model" -o "$out/$name" "$t" \
        "$root/test/model/ref_fsm89.c" \
        "$root/test/support/fsm89_gen.c" \
        "$root/test/support/fsm89_test.c" \
        "$root/test/support/fsm89_fixtures.c" "$out/libfsm89.a"
    ASAN_OPTIONS=detect_leaks=1 "$out/$name" > /dev/null
done

echo "sanitize: ok"
