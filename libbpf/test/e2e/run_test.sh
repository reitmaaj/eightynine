#!/bin/sh -eu
# e2e: bpf run executes a program and prints r0.
bpf="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# r0 = 42; exit
printf '\xb7\x00\x00\x00\x2a\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/add.bpf"

out=$("$bpf" run "$tmp/add.bpf")
test "$out" = "42"

# A truncated program (wide LD with only 8 bytes) must be rejected.
printf '\x18\x00\x00\x00\x01\x00\x00\x00' >"$tmp/trunc.bpf"
if "$bpf" run "$tmp/trunc.bpf" >/dev/null 2>&1; then
    echo "FAIL: truncated program accepted" >&2
    exit 1
fi

# A program with no EXIT must be rejected (falls off the end).
printf '\xb7\x00\x00\x00\x01\x00\x00\x00' >"$tmp/noexit.bpf"
if "$bpf" run "$tmp/noexit.bpf" >/dev/null 2>&1; then
    echo "FAIL: no-exit program accepted" >&2
    exit 1
fi

echo "PASS: bpf run"
