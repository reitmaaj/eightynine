#!/bin/sh -eu
# e2e: bpf groups golden output across conformance groups.
bpf="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# 32-bit only: MOV32 then exit -> base32 only.
printf '\xb4\x00\x00\x00\x01\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/p32.bpf"
g32=$("$bpf" groups "$tmp/p32.bpf")
test "$g32" = "base32"

# 64-bit ALU -> base32 + base64.
printf '\xb7\x00\x00\x00\x2a\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/p64.bpf"
g64=$("$bpf" groups "$tmp/p64.bpf")
printf '%s\n' "$g64" | grep -q '^base32$'
printf '%s\n' "$g64" | grep -q '^base64$'

# atomic32 -> atomic32 reported.
printf '\xb7\x01\x00\x00\x00\x00\x00\x00\xb7\x02\x00\x00\x01\x00\x00\x00\xc3\x12\x00\x00\x00\x00\x00\x00\x61\x10\x00\x00\x00\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/patomic.bpf"
gat=$("$bpf" groups "$tmp/patomic.bpf")
printf '%s\n' "$gat" | grep -q '^atomic32$'

echo "PASS: bpf groups golden"
