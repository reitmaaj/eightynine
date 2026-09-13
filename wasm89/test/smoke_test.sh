#!/bin/sh -eu
out=$(./build/wasm89 version)
test "$out" = "1"
echo "PASS: wasm89 version prints 1"
