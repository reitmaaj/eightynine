#!/bin/sh -eu
set -eu
# sanitize.sh - rebuild libsyntax89 and the fast suite with ASan + UBSan, then
# run every test binary under the sanitizers.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/asan"
cc="${ASAN_CC:-clang}"

mkdir -p "$out"

set -- -std=c99 -pedantic-errors -Wall -Wextra -Werror
set -- "$@" -fsanitize=address,undefined -fno-sanitize-recover=all
set -- "$@" -fno-omit-frame-pointer -g

cat > "$out/selftest_ub.c" <<'EOF'
int main(void)
{
    int x = 2147483647;
    int y;

    y = x + 1;
    return y == 0;
}
EOF

cat > "$out/selftest_asan.c" <<'EOF'
#include <stdlib.h>

int main(void)
{
    char *p;

    p = (char *)malloc(4);
    p[4] = 1;
    free(p);
    return 0;
}
EOF

# Self-check: a known undefined behavior and a known address error must fail
# their runs. A recovering UBSan would make this gate vacuous.
"$cc" "$@" -o "$out/selftest_ub" "$out/selftest_ub.c"
if "$out/selftest_ub" >/dev/null 2>&1; then
    echo "sanitize: self-check failed: undefined behavior was not fatal" >&2
    exit 1
fi

"$cc" "$@" -o "$out/selftest_asan" "$out/selftest_asan.c"
if "$out/selftest_asan" >/dev/null 2>&1; then
    echo "sanitize: self-check failed: address error was not fatal" >&2
    exit 1
fi

rm -f "$out"/*.o "$out"/libsyntax89.a
for f in "$root"/src/*.c; do
    name=$(basename "$f" .c)
    "$cc" "$@" -I"$root/include" -c "$f" -o "$out/$name.o"
done
ar rcs "$out/libsyntax89.a" "$out"/*.o

for t in "$root"/test/smoke.c "$root"/test/unit/test_*.c \
    "$root"/test/model/test_*.c "$root"/test/generated/test_*.c \
    "$root"/test/fault/test_*.c "$root"/test/stress/test_*.c; do
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
    ASAN_OPTIONS=detect_leaks=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$out/$name" > /dev/null
done

echo "sanitize: ok"
