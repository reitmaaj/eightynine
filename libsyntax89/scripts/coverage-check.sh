#!/bin/sh -eu
set -eu
# coverage-check.sh <src-dir> <gcov-dir>
#
# Require one .gcov report per source file and no unexecuted line or branch.

src="$1"
gcovdir="$2"
found=0
fail=0

for f in "$src"/*.c; do
    [ -e "$f" ] || continue
    found=1
    name=$(basename "$f" .c)
    report="$gcovdir/$name.c.gcov"
    if [ ! -s "$report" ]; then
        echo "coverage-check: missing report $report" >&2
        fail=1
    fi
done

if [ "$found" -eq 0 ]; then
    echo "coverage-check: no sources under $src" >&2
    exit 1
fi

for report in "$gcovdir"/*.gcov; do
    [ -e "$report" ] || continue
    if grep -q '^ *#####' "$report"; then
        echo "coverage-check: unexecuted line in $report" >&2
        fail=1
    fi
    if grep -q 'never executed' "$report"; then
        echo "coverage-check: unexecuted branch in $report" >&2
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    exit 1
fi

echo "coverage-check: clean"
