#!/bin/sh -eu
# rfc8259.sh - focused conformance cases distilled from RFC 8259 (see
# rfc8259.txt at the repo root). libj89 deviations from the full grammar
# (floats, exponents, and lone surrogates) are asserted as documented.

J89="$1"

failures=0

case_accept() {
    label="$1"
    input="$2"
    if printf '%s' "$input" | "$J89" >/dev/null 2>&1; then
        return 0
    fi
    echo "FAIL: [$label] expected accept: $(printf '%s' "$input")" >&2
    return 1
}

case_reject() {
    label="$1"
    input="$2"
    if ! printf '%s' "$input" | "$J89" >/dev/null 2>&1; then
        return 0
    fi
    echo "FAIL: [$label] expected reject: $(printf '%s' "$input")" >&2
    return 1
}

# --- RFC 3: values ---
case_accept "true" 'true' || failures=$((failures + 1))
case_accept "false" 'false' || failures=$((failures + 1))
case_accept "null" 'null' || failures=$((failures + 1))
case_reject "True" 'True' || failures=$((failures + 1))
case_reject "FALSE" 'FALSE' || failures=$((failures + 1))
case_reject "NULL" 'NULL' || failures=$((failures + 1))
case_reject "tru" 'tru' || failures=$((failures + 1))
case_reject "nul" 'nul' || failures=$((failures + 1))
case_reject "undefined" 'undefined' || failures=$((failures + 1))

# --- RFC 4: objects ---
case_accept "empty object" '{}' || failures=$((failures + 1))
case_accept "object" '{"a":1}' || failures=$((failures + 1))
case_accept "nested object" '{"a":{"b":null}}' || failures=$((failures + 1))
case_reject "missing colon" '{"a" 1}' || failures=$((failures + 1))
case_reject "missing value" '{"a":}' || failures=$((failures + 1))
case_reject "trailing comma" '{"a":1,}' || failures=$((failures + 1))
case_reject "unquoted key" '{a:1}' || failures=$((failures + 1))
case_reject "single-quote key" "{'a':1}" || failures=$((failures + 1))
case_reject "duplicate key" '{"a":1,"a":2}' || failures=$((failures + 1))

# --- RFC 5: arrays ---
case_accept "empty array" '[]' || failures=$((failures + 1))
case_accept "heterogeneous array" '[true,false,null,1,"s",[],{}]' \
    || failures=$((failures + 1))
case_reject "trailing comma array" '[1,]' || failures=$((failures + 1))
case_reject "leading comma array" '[,1]' || failures=$((failures + 1))
case_reject "missing comma" '[1 2]' || failures=$((failures + 1))

# --- RFC 6: numbers ---
case_accept "zero" '0' || failures=$((failures + 1))
case_accept "negative zero" '-0' || failures=$((failures + 1))
case_accept "positive int" '42' || failures=$((failures + 1))
case_accept "negative int" '-42' || failures=$((failures + 1))
case_accept "large int" '1234567890' || failures=$((failures + 1))
case_accept "2^53" '9007199254740992' || failures=$((failures + 1))
case_reject "leading zero" '01' || failures=$((failures + 1))
case_reject "negative leading zero" '-01' || failures=$((failures + 1))
case_reject "double leading zero" '00' || failures=$((failures + 1))
case_reject "bare minus" '-' || failures=$((failures + 1))
case_reject "integer above 2^53" '9007199254740993' || failures=$((failures + 1))
case_reject "integer below -2^53" '-9007199254740993' || failures=$((failures + 1))
case_reject "LONG_MAX" '9223372036854775807' || failures=$((failures + 1))
case_reject "LONG_MIN" '-9223372036854775808' || failures=$((failures + 1))
case_accept "float" '1.5' || failures=$((failures + 1))
case_accept "negative float" '-0.5' || failures=$((failures + 1))
case_accept "exponent e" '1e3' || failures=$((failures + 1))
case_accept "exponent E" '1E3' || failures=$((failures + 1))
case_accept "exponent plus" '1e+3' || failures=$((failures + 1))
case_accept "exponent minus" '1e-3' || failures=$((failures + 1))
case_accept "large exponent" '123e65' || failures=$((failures + 1))
case_reject "dot then nothing" '1.' || failures=$((failures + 1))
case_reject "dot leading" '.5' || failures=$((failures + 1))
case_reject "exponent bare" '1e' || failures=$((failures + 1))
case_reject "exponent bare plus" '1e+' || failures=$((failures + 1))
case_reject "overflow double" '1e999' || failures=$((failures + 1))
case_reject "infinity" 'Infinity' || failures=$((failures + 1))
case_reject "nan" 'NaN' || failures=$((failures + 1))

# --- RFC 7: strings ---
case_accept "empty string" '""' || failures=$((failures + 1))
case_accept "basic string" '"hello"' || failures=$((failures + 1))
case_accept "escapes" '"a\"b\\c\/d\b\f\n\r\t"' || failures=$((failures + 1))
case_accept "unicode escape" '"\u0041"' || failures=$((failures + 1))
case_accept "surrogate pair" '"\ud834\udd1e"' || failures=$((failures + 1))
case_accept "line separator U+2028" '"x\u2028y"' || failures=$((failures + 1))
case_reject "unterminated" '"abc' || failures=$((failures + 1))
case_reject "bad escape" '"\x"' || failures=$((failures + 1))
case_reject "short unicode escape" '"\u12"' || failures=$((failures + 1))
case_reject "invalid hex escape" '"\uZZZZ"' || failures=$((failures + 1))
case_reject "lone high surrogate" '"\ud800"' || failures=$((failures + 1))
case_reject "lone low surrogate" '"\udc00"' || failures=$((failures + 1))
case_reject "single quote string" "'abc'" || failures=$((failures + 1))

# Raw control characters (tab, newline) inside a string MUST be escaped.
case_reject "raw tab in string" "$(printf '"a\tb"')" || failures=$((failures + 1))
case_reject "raw newline in string" "$(printf '"a\nb"')" || failures=$((failures + 1))
case_accept "escaped tab" '"a\tb"' || failures=$((failures + 1))

# --- RFC 8: a leading UTF-8 BOM may be ignored; libj89 ignores it. ---
case_accept "BOM prefix" "$(printf '\357\273\277{}')" || failures=$((failures + 1))
case_reject "partial BOM" "$(printf '\357{}')" || failures=$((failures + 1))

# --- RFC 2: whitespace is insignificant around structural chars. ---
case_accept "space padding" '  [ 1 , 2 ]  ' || failures=$((failures + 1))
case_accept "tab newline cr padding" "$(printf '{\n\t"a"\r:\n1\n}')" \
    || failures=$((failures + 1))
case_reject "vertical tab is not ws" "$(printf '[1\v2]')" || failures=$((failures + 1))
case_reject "form feed is not ws" "$(printf '[1\f2]')" || failures=$((failures + 1))

# --- RFC 2/9: trailing non-whitespace after the value is rejected. ---
case_accept "trailing whitespace ok" '1 ' || failures=$((failures + 1))
case_reject "trailing garbage" '1 2' || failures=$((failures + 1))
case_reject "trailing comma root" '[1] x' || failures=$((failures + 1))

if [ "$failures" -ne 0 ]; then
    echo "rfc8259: $failures failure(s)" >&2
    exit 1
fi
echo "rfc8259: ok"
