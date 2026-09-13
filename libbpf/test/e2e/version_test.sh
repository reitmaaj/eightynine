#!/bin/sh -eu
# e2e: bpf version and missing-file handling.
bpf="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# version prints 1.
v=$("$bpf" version)
test "$v" = "1"

# Missing file is an error for each subcommand.
for sub in run dis groups; do
    if "$bpf" "$sub" "$tmp/nonexistent" >/dev/null 2>&1; then
        echo "FAIL: $sub accepted missing file" >&2
        exit 1
    fi
done

# A valid program runs.
printf '\xb7\x00\x00\x00\x07\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/p.bpf"
out=$("$bpf" run "$tmp/p.bpf")
test "$out" = "7"

# dis lists the two instructions.
n=$("$bpf" dis "$tmp/p.bpf" | wc -l)
test "$n" = "2"

echo "PASS: bpf version and file handling"
