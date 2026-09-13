#!/bin/sh -eu
# alg-audit.sh - source/dependency opacity and export audits for j89_alg.
#
# Usage:
#   alg-audit.sh forbidden   - forbid policy/semantic dependencies in src/alg
#   alg-audit.sh exports     - flag derived-looking exported symbols
#
# Scope: src/alg must not pull in scalar semantics or a parser/renderer, and
# the exported symbol surface must not expose derived (policy) operations.

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
ALG="$ROOT/src/alg"
status=0

forbidden_symbols='strcmp|memcmp|strstr|strchr|strtod|strtol|atof|atoi|isspace|tolower|toupper'
forbidden_headers='math\.h|ctype\.h|regex\.h'
derived_tokens='lookup|_get|_set|_find|keys|values|filter|merge|search|path|index|slice|reverse|remove|replace|_eq|compare|_sort|_count|parse|render'

forbidden()
{
    found=0
    if grep -rEn "$forbidden_headers" "$ALG"; then
        found=1
    fi
    if grep -rEn "$forbidden_symbols" "$ALG"; then
        found=1
    fi
    if grep -rEn 'j89_parse|j89_render|j89_object_find|j89_array_get' "$ALG"; then
        found=1
    fi
    if [ "$found" -ne 0 ]; then
        echo "alg-audit: forbidden scalar/parser dependency detected" >&2
        status=1
    else
        echo "alg-audit forbidden: clean"
    fi
}

exports()
{
    bad=0
    count=0
    for f in "$ALG"/*.c; do
        name=$(basename "$f" .c)
        obj="/tmp/alg-audit-$name.o"
        cc -std=c89 -I"$ROOT/include" -I"$ROOT/src" -c "$f" -o "$obj" || exit 1
    done
    symbols=$(nm --defined-only /tmp/alg-audit-*.o | awk '$2 ~ /^[TDBR]$/ {print $3}')
    for s in $symbols; do
        count=$((count + 1))
        case "$s" in
            j89a_*) ;;
            *)
                echo "alg-audit: non-j89a symbol exported: $s" >&2
                bad=1
                ;;
        esac
        if echo "$s" | grep -qE "j89a_.*($derived_tokens)"; then
            echo "alg-audit: derived-sounding symbol: $s" >&2
            bad=1
        fi
    done
    rm -f /tmp/alg-audit-*.o
    if [ "$bad" -ne 0 ]; then
        status=1
    else
        echo "alg-audit exports: clean ($count symbols)"
    fi
}

case "${1:-}" in
    forbidden) forbidden ;;
    exports) exports ;;
    *)
        echo "usage: alg-audit.sh <forbidden|exports>" >&2
        status=2
        ;;
esac

exit "$status"
