#!/bin/sh -eu
set -eu
# coverage-selftest.sh - prove the coverage gate rejects incomplete data.
#
# The gate must fail on a missing report, an unexecuted line, an unexecuted
# branch, and an empty source set, and succeed on a complete report.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
check="$root/scripts/coverage-check.sh"
tmp="$(mktemp -d)"

cleanup() {
    rm -rf "$tmp"
}
trap cleanup 0 1 2 15

src="$tmp/src"
gcovdir="$tmp/gcov"
empty="$tmp/empty"

mkdir -p "$src" "$gcovdir" "$empty"
printf 'int a(void) { return 0; }\n' > "$src/a.c"
printf 'int b(void) { return 0; }\n' > "$src/b.c"
printf '        -:    0:Source:a.c\n' > "$gcovdir/a.c.gcov"
printf '        -:    0:Source:b.c\n' > "$gcovdir/b.c.gcov"

sh "$check" "$src" "$gcovdir"

rm "$gcovdir/b.c.gcov"
if sh "$check" "$src" "$gcovdir" >/dev/null 2>&1; then
    echo "coverage-selftest: missing report accepted" >&2
    exit 1
fi
printf '        -:    0:Source:b.c\n' > "$gcovdir/b.c.gcov"

printf '        -:    0:Source:b.c\n    #####:    1:return 0;\n' \
    > "$gcovdir/b.c.gcov"
if sh "$check" "$src" "$gcovdir" >/dev/null 2>&1; then
    echo "coverage-selftest: unexecuted line accepted" >&2
    exit 1
fi

printf '        -:    0:Source:b.c\nbranch  0 never executed\n' \
    > "$gcovdir/b.c.gcov"
if sh "$check" "$src" "$gcovdir" >/dev/null 2>&1; then
    echo "coverage-selftest: unexecuted branch accepted" >&2
    exit 1
fi

if sh "$check" "$empty" "$gcovdir" >/dev/null 2>&1; then
    echo "coverage-selftest: empty source set accepted" >&2
    exit 1
fi

echo "coverage-selftest: ok"
