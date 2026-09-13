#!/bin/sh -eu
set -eu
# sanitize.sh - rebuild libcksum89 and the unit suite with ASan + UBSan and
# run every test binary under the sanitizers.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/build/asan"
CC=${ASAN_CC:-clang}

mkdir -p "$OUT"

set -- -std=c99 -pedantic-errors -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g

rm -f "$OUT"/*.o "$OUT"/test_cksum89 "$OUT"/smoke
for f in "$ROOT"/src/*.c; do
    [ -e "$f" ] || continue
    name=$(basename "$f" .c)
    "$CC" "$@" -I"$ROOT/include" -I"$ROOT/src" -c "$f" -o "$OUT/lib_$name.o"
done
for f in "$ROOT"/test/*.c; do
    [ -e "$f" ] || continue
    case "$f" in
        */smoke.c) continue ;;
    esac
    name=$(basename "$f" .c)
    "$CC" "$@" -Wno-unused-function -I"$ROOT/include" -I"$ROOT/src" \
        -I"$ROOT/test" -c "$f" -o "$OUT/test_$name.o"
done
"$CC" "$@" -Wno-unused-function -I"$ROOT/include" -I"$ROOT/src" \
    -I"$ROOT/test" "$OUT"/lib_*.o "$OUT"/test_*.o -o "$OUT/test_cksum89"
"$OUT/test_cksum89"
if [ -e "$ROOT/test/smoke.c" ]; then
    "$CC" "$@" -Wno-unused-function -I"$ROOT/include" -I"$ROOT/src" \
        -I"$ROOT/test" "$ROOT/test/smoke.c" "$OUT"/lib_*.o -o "$OUT/smoke"
    "$OUT/smoke"
fi
echo "sanitize: ok"
