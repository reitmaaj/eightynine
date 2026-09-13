#!/bin/sh -eu
# fuzz.sh - generated adversarial inputs for the j89 CLI.
#
# Every input must be handled by accepting (exit 0) or rejecting (exit 1);
# a process signal (exit >= 128) is always a failure. Deterministic.

J89="$1"

CDIR=$(dirname "$0")

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

# Check that an input file is handled without crashing. `expect` is optional:
# if given (0 or 1) the exact exit code is required.
assert_handled() {
    file="$1"
    expect="${2-}"
    "$J89" "$file" >/dev/null 2>/dev/null
    rc=$?
    if [ "$rc" -ge 128 ]; then
        echo "FAIL: $file crashed with signal (rc=$rc)" >&2
        return 1
    fi
    if [ -n "$expect" ]; then
        if [ "$expect" -eq 0 ] && [ "$rc" -ne 0 ]; then
            echo "FAIL: $file expected accept, got rc=$rc" >&2
            return 1
        fi
        if [ "$expect" -eq 1 ] && [ "$rc" -ne 1 ]; then
            echo "FAIL: $file expected reject, got rc=$rc" >&2
            return 1
        fi
    fi
    return 0
}

# Emit `n` copies of the byte `ch`.
rep() {
    n="$1"
    ch="$2"
    awk -v n="$n" -v ch="$ch" 'BEGIN { for (i = 0; i < n; i++) printf "%s", ch }'
}

failures=0

# --- Depth ladder: valid balanced nesting at increasing depth. ---
i=0
for d in 100 1000 5000 20000 50000 100000; do
    rep "$d" '[' >"$TMP/depth_$d.json"
    rep "$d" ']' >>"$TMP/depth_$d.json"
    # A reasonable depth must be accepted; deep nesting may be rejected by a
    # documented limit, but must never crash.
    if [ "$d" -le 1000 ]; then
        assert_handled "$TMP/depth_$d.json" 0 || failures=$((failures + 1))
    else
        assert_handled "$TMP/depth_$d.json" || failures=$((failures + 1))
    fi
    i=$((i + 1))
done

# --- Unterminated deep nesting: must reject (rc=1) without crashing. ---
for d in 1000 50000 100000; do
    rep "$d" '[' >"$TMP/unterm_$d.json"
    assert_handled "$TMP/unterm_$d.json" 1 || failures=$((failures + 1))
done

# --- Integer overflow: must reject cleanly. ---
rep 30 '9' >"$TMP/bigint.json"
assert_handled "$TMP/bigint.json" 1 || failures=$((failures + 1))
printf '%s' '-999999999999999999999999' >"$TMP/bigneg.json"
assert_handled "$TMP/bigneg.json" 1 || failures=$((failures + 1))

# --- Long string value: must accept. ---
{
    printf '"'
    rep 100000 'a'
    printf '"'
} >"$TMP/longstr.json"
assert_handled "$TMP/longstr.json" 0 || failures=$((failures + 1))

# --- Object with many members: must accept. ---
awk 'BEGIN {
    printf "{";
    for (i = 0; i < 2000; i++) {
        if (i > 0) printf ",";
        printf "\"k%d\":%d", i, i;
    }
    printf "}";
}' >"$TMP/manykeys.json"
assert_handled "$TMP/manykeys.json" 0 || failures=$((failures + 1))

# --- Truncations of a valid corpus file: must never crash. ---
SRC="$CDIR/../json/y_object_long_strings.json"
if [ -f "$SRC" ]; then
    size=$(wc -c <"$SRC")
    for frac in 1 2 3 4 8 16; do
        cut=$((size / frac))
        head -c "$cut" "$SRC" >"$TMP/trunc_$frac.json"
        assert_handled "$TMP/trunc_$frac.json" || failures=$((failures + 1))
    done
fi

# --- Seeded random fuzz over a JSON-ish alphabet: must never crash. ---
# One generated input per line; each line becomes a separate file.
awk 'BEGIN {
    srand(42);
    alphabet = "{}[],:\"\\-0123456789abcdefghijklmnopqrstuvwxyz \t\n";
    len = length(alphabet);
    for (k = 0; k < 400; k++) {
        n = int(rand() * 512);
        line = "";
        for (i = 0; i < n; i++) {
            j = int(rand() * len) + 1;
            line = line substr(alphabet, j, 1);
        }
        printf "%s\n", line;
    }
}' >"$TMP/fuzz.txt"

awk -v out="$TMP" 'BEGIN { n = 0 } { fn = out "/fuzz_" n ".json"; n = n + 1; print > fn }' "$TMP/fuzz.txt"
for f in "$TMP"/fuzz_*.json; do
    assert_handled "$f" || failures=$((failures + 1))
done

if [ "$failures" -ne 0 ]; then
    echo "fuzz: $failures failure(s)" >&2
    exit 1
fi
echo "fuzz: ok"
