#!/bin/sh -eu
# Assert that non-C89 source fails to compile under the strict flags.
cc="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror \
    -Wconversion -Wsign-conversion -Wstrict-prototypes \
    -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2

# A // comment is not valid C89.
printf 'int x; // c89-invalid\n' >"$tmp/slash.c"
if "$cc" "$@" -c "$tmp/slash.c" -o "$tmp/slash.o" 2>/dev/null; then
    echo "FAIL: // comment compiled under strict C89" >&2
    exit 1
fi

# A declaration after a statement is not valid C89.
printf 'void f(void) { int a; a = 1; int b; b = a; }\n' >"$tmp/late.c"
if "$cc" "$@" -c "$tmp/late.c" -o "$tmp/late.o" 2>/dev/null; then
    echo "FAIL: late declaration compiled under strict C89" >&2
    exit 1
fi

echo "PASS: strict C89 rejects non-conforming source"
