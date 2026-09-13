#!/bin/sh -eu
out=$(./build/bpf version)
test "$out" = "1"
echo "PASS: bpf version prints 1"
