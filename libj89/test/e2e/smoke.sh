#!/bin/sh -eu
# e2e smoke test for the j89 CLI: valid inputs round-trip, invalid are rejected.

J89="$1"

assert_ok() {
    input="$1"
    expected="$2"
    got=$(printf '%s' "$input" | "$J89")
    if [ "$got" != "$expected" ]; then
        echo "FAIL: expected [$expected] got [$got]" >&2
        exit 1
    fi
}

assert_fail() {
    input="$1"
    if printf '%s' "$input" | "$J89" >/dev/null 2>&1; then
        echo "FAIL: expected rejection of [$input]" >&2
        exit 1
    fi
}

assert_ok '{"a":1}' '{"a":1}'
assert_ok '[1,2,3]' '[1,2,3]'
assert_ok 'true' 'true'
assert_ok 'null' 'null'
assert_ok '"hi"' '"hi"'
assert_ok '{"b":[true,null],"c":{"d":-3}}' '{"b":[true,null],"c":{"d":-3}}'
assert_ok '1.5' '1.5'
assert_ok '1e3' '1000'
assert_ok '123e65' '1.2300000000000001e+67'

assert_fail '1.'
assert_fail '.5'
assert_fail '1e'
assert_fail '1e999'
assert_fail '01'
assert_fail '{} x'
assert_fail '{"a":}'
assert_fail '{"a":1,"a":2}'
assert_fail '"abc'
assert_fail '[,]'
assert_fail '{'

# Raw and escaped Unicode must render identically (libu89 delegation).
raw_e=$(printf '"\303\251"')
esc_e='"\u00e9"'
got_raw=$(printf '%s' "$raw_e" | "$J89")
got_esc=$(printf '%s' "$esc_e" | "$J89")
if [ "$got_raw" != "$got_esc" ]; then
    echo "FAIL: raw and escaped U+00E9 differ" >&2
    exit 1
fi

raw_emoji=$(printf '"\360\237\230\200"')
esc_emoji='"\ud83d\ude00"'
got_raw=$(printf '%s' "$raw_emoji" | "$J89")
got_esc=$(printf '%s' "$esc_emoji" | "$J89")
if [ "$got_raw" != "$got_esc" ]; then
    echo "FAIL: raw and escaped U+1F600 differ" >&2
    exit 1
fi

# Malformed raw UTF-8 must be rejected (delegated validity).
assert_fail "$(printf '"\303"')"
assert_fail "$(printf '"\355\240\200"')"

echo "e2e: ok"
