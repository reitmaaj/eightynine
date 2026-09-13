#!/bin/sh -eu
# Smoke test for the `wasm89 repl` line protocol.

wasm89=./build/wasm89
fixture=test/fixtures/add.wasm

out=$("$wasm89" repl <<EOF
module $fixture
invoke last add 2 i32:0x5 i32:0x7
assert_return last add 2 i32:0x5 i32:0x7 1 i32:0xc
assert_return last add 2 i32:0x5 i32:0x7 1 i32:0xd
get last g
assert_return_get last g i32:0x7
assert_trap last boom 0 trap
register M
module $fixture
quit
EOF
)

check() {
    printf '%s\n' "$out" | grep -qx "$1" || {
        printf 'FAIL: missing output line: %s\n' "$1"
        printf '%s\n' "$out"
        exit 1
    }
}

check '@ok'
check '@return i32:0xc'
check '@pass'
check '@fail i32:0xc'
check '@return i32:0x7'
check '@fail unknown function'
check '@ok'
printf 'PASS: repl smoke test\n'
