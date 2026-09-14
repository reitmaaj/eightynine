#!/bin/sh -eu
set -eu
# compile-check.sh - header hygiene and const-correctness compile probes.
#
# Positive probes must compile (and the linked ones must run). Negative
# probes must fail to compile.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/compile"
cc="${CC:-cc}"

mkdir -p "$out"

set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -I"$root/include"

"$cc" "$@" -c "$root/test/compile/header_alone.c" -o "$out/header_alone.o"
"$cc" "$@" -c "$root/test/compile/double_include.c" \
    -o "$out/double_include.o"
"$cc" "$@" -c "$root/test/compile/before_std.c" -o "$out/before_std.o"
"$cc" "$@" -c "$root/test/compile/after_std.c" -o "$out/after_std.o"
"$cc" "$@" -c "$root/test/compile/two_tu_a.c" -o "$out/two_tu_a.o"
"$cc" "$@" -c "$root/test/compile/two_tu_b.c" -o "$out/two_tu_b.o"
"$cc" "$@" "$out/two_tu_a.o" "$out/two_tu_b.o" "$root/build/libsyntax89.a" \
    -o "$out/two_tu"
"$out/two_tu"

"$cc" "$@" -c "$root/test/compile/const_ok.c" -o "$out/const_ok.o"
"$cc" "$@" "$out/const_ok.o" "$root/build/libsyntax89.a" -o "$out/const_ok"
"$out/const_ok"

if "$cc" "$@" -c "$root/test/compile/reject_write_through_graph.c" \
    -o "$out/reject_graph.o" >/dev/null 2>&1; then
    echo "compile-check: write through const graph compiled but must not"
    exit 1
fi

if "$cc" "$@" -c "$root/test/compile/reject_write_through_allocator.c" \
    -o "$out/reject_alloc.o" >/dev/null 2>&1; then
    echo "compile-check: write through const allocator compiled but must not"
    exit 1
fi

echo "compile-check: ok"
