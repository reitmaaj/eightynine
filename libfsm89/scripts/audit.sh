#!/bin/sh -eu
set -eu
# audit.sh - symbol audit for libfsm89.
#
# 1. The archive must reference no external symbol at all (no allocation, no
#    I/O, no threading); the only undefined names may be the documented
#    internal fsm89__ helpers resolved inside the archive.
# 2. The exported symbol set must be exactly the four public functions plus
#    the three documented internal helpers.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
lib="$root/build/libfsm89.a"

if [ ! -e "$lib" ]; then
    echo "audit: $lib missing; run 'just build' first"
    exit 1
fi

external=$(nm -u "$lib" | awk 'NF >= 2 { print $NF }' |
    grep -v -E '^fsm89__[a-z_]+$' || true)
if [ -n "$external" ]; then
    echo "audit: unexpected undefined symbols:"
    printf '%s\n' "$external"
    exit 1
fi

expected="fsm89__find_edge
fsm89__find_state
fsm89__span_ok
fsm89_accepting
fsm89_start
fsm89_step
fsm89_validate"

actual=$(nm -g --defined-only "$lib" | awk 'NF >= 3 { print $NF }' | sort -u)
want=$(printf '%s\n' "$expected" | sort -u)

if [ "$actual" != "$want" ]; then
    echo "audit: exported symbol set mismatch"
    echo "--- actual ---"
    printf '%s\n' "$actual"
    echo "--- expected ---"
    printf '%s\n' "$want"
    exit 1
fi

echo "audit: ok"
