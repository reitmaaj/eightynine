#!/bin/sh -eu
# corpus.sh - stress-run the j89 CLI over the vendored JSONTestSuite corpus.
#
# Expected outcomes:
#   n_*.json             must reject with exit code 1 (never 0, 2, or crash)
#   y_*.json             must accept (exit 0) unless listed in Y_REJECT
#   i_*.json             must match the pinned baseline (I_ACCEPT/I_REJECT)
#
# A signal (exit >= 128) or a timeout is always a hard failure.

J89="$1"

# Directory holding the corpus files.
CDIR=$(dirname "$0")
JSONDIR="$CDIR/../json"

# y_* files that are valid RFC 8259 JSON but lie outside the libj89 subset
# and therefore MUST be rejected. After the RFC 8259 conformance change, the
# only such files are objects with duplicate keys (rejected by policy).
Y_REJECT="
y_object_duplicated_key.json
y_object_duplicated_key_and_value.json
"

# i_* files pinned as ACCEPTED (the rest of i_* are pinned as rejected).
I_ACCEPT="
i_structure_500_nested_arrays.json
i_structure_UTF-8_BOM_empty_object.json
"

in_list() {
    name="$1"
    list="$2"
    if printf '%s\n' "$list" | grep -qx "$name"; then
        return 0
    fi
    return 1
}

run_one() {
    file="$1"
    expect="$2" # 0 = accept, 1 = reject
    base=$(basename "$file")
    "$J89" "$file" >/dev/null 2>/dev/null
    rc=$?
    if [ "$rc" -ge 128 ]; then
        echo "FAIL: $base crashed with signal (rc=$rc), expected reject-or-accept" >&2
        return 1
    fi
    if [ "$expect" -eq 0 ]; then
        if [ "$rc" -ne 0 ]; then
            echo "FAIL: $base expected accept, got rc=$rc" >&2
            return 1
        fi
    else
        if [ "$rc" -ne 1 ]; then
            echo "FAIL: $base expected reject (rc=1), got rc=$rc" >&2
            return 1
        fi
    fi
    return 0
}

failures=0

for file in "$JSONDIR"/n_*.json; do
    [ -e "$file" ] || continue
    run_one "$file" 1 || failures=$((failures + 1))
done

for file in "$JSONDIR"/y_*.json; do
    [ -e "$file" ] || continue
    base=$(basename "$file")
    if in_list "$base" "$Y_REJECT"; then
        run_one "$file" 1 || failures=$((failures + 1))
    else
        run_one "$file" 0 || failures=$((failures + 1))
    fi
done

for file in "$JSONDIR"/i_*.json; do
    [ -e "$file" ] || continue
    base=$(basename "$file")
    if in_list "$base" "$I_ACCEPT"; then
        run_one "$file" 0 || failures=$((failures + 1))
    else
        run_one "$file" 1 || failures=$((failures + 1))
    fi
done

if [ "$failures" -ne 0 ]; then
    echo "corpus: $failures failure(s)" >&2
    exit 1
fi
echo "corpus: ok ($(find "$JSONDIR" -maxdepth 1 -name '*.json' | wc -l) files)"
