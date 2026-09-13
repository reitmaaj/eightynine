#!/bin/sh -eu
# Report and assert the test:source LOC ratio (target >= 3:1).
# Source = src/*.c implementation; test = all test/*.c and test/*.sh.
src=$(cat src/*.c 2>/dev/null | wc -l)
test=$(cat test/unit/*.c test/e2e/*.sh test/*.sh 2>/dev/null | wc -l)
echo "source LOC: $src"
echo "test   LOC: $test"
awk -v s="$src" -v t="$test" 'BEGIN {
    if (s == 0) { print "FAIL: no source"; exit 1 }
    r = t / s;
    printf "ratio: %.3f : 1 (target >= 3)\n", r;
    if (r < 3.0) { print "FAIL: ratio below 3:1"; exit 1 }
    print "ratio gate: OK";
}'
