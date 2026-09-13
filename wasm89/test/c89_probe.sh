#!/bin/sh -eu
cc="$1"
flags="$2"
# shellcheck disable=SC2086
if $cc $flags -c test/not_c89_probe.c -o /dev/null 2>/dev/null; then
    echo "FAIL: non-C89 source compiled without error" >&2
    exit 1
fi
echo "PASS: non-C89 source rejected"
