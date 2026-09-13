#!/bin/sh -eu
# e2e: bpf dis and bpf groups.
bpf="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# r0 = 42; exit
printf '\xb7\x00\x00\x00\x2a\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/add.bpf"

lines=$("$bpf" dis "$tmp/add.bpf")
count=$(printf '%s\n' "$lines" | wc -l)
test "$count" = "2"

groups=$("$bpf" groups "$tmp/add.bpf")
printf '%s\n' "$groups" | grep -q '^base32$'
printf '%s\n' "$groups" | grep -q '^base64$'

echo "PASS: bpf dis and groups"
