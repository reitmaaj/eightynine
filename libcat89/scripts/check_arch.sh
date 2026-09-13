#!/bin/sh -eu
# check_arch.sh - architecture / ABI hygiene gates (W7).
#
# Hard failures:
#   1. core.h includes no derived header (its <cat89/...> includes must be
#      within {core.h, alloc.h, status.h}) - CAT-I2 / core freeze.
#   2. The public header include graph is acyclic (no layering cycle).
#   3. A translation unit including ONLY <cat89/core.h> compiles standalone in
#      strict C89 (core drags in nothing higher).
#   4. Every global symbol defined by the archive is exported only through a
#      public header (or cat89_internal.h) and carries the cat89_ prefix - no
#      accidental ABI surface or namespace pollution.
#
# Informational: a listing of concrete struct definitions that appear in public
# headers (i.e. exposed layouts) for a human ABI review.

root="$(cd "$(dirname "$0")/.." && pwd)"
inc="$root/include/cat89"
src="$root/src"
cc=${CC:-cc}
strict="-std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# ---- 1. core.h isolation -------------------------------------------------
core_ok=1
# shellcheck disable=SC2013
for incf in $(grep -o '#include <cat89/[a-zA-Z0-9_.]*\.h>' "$inc/core.h" | sed 's/#include <cat89\///; s/>//'); do
    case "$incf" in
        core.h|alloc.h|status.h) : ;;
        *) printf 'ARCH FAIL: core.h must not include derived header <%s>\n' "$incf"; core_ok=0 ;;
    esac
done
if [ "$core_ok" -eq 0 ]; then
    exit 1
fi

# ---- 2. header include-graph acyclicity (Kahn, via awk) -------------------
# nodes: every header basename; edges: file -> its <cat89/...> includes.
"$root"/scripts/gen_edges.awk "$inc" > "$tmp/edges.txt"
"$root"/scripts/acyclic.awk "$tmp/edges.txt"
echo "ARCH PASS: header include graph is acyclic"

# ---- 3. core-only translation unit ----------------------------------------
# A TU whose only cat89 include is <cat89/core.h> must parse clean in strict
# C89: core drags in nothing derived/higher than alloc.h/status.h.
printf 'int cat89_arch_core_probe(void);\n#include <cat89/core.h>\nint cat89_arch_core_probe(void)\n{\n    return 0;\n}\n' > "$tmp/core_only.c"
# shellcheck disable=SC2086
if ! $cc $strict -I"$root/include" -c "$tmp/core_only.c" -o "$tmp/core_only.o"; then
    echo "ARCH FAIL: core-only TU failed to compile clean in strict C89" >&2
    exit 1
fi
echo "ARCH PASS: core-only TU compiles clean in strict C89"

# ---- 4. exported-symbol inventory ----------------------------------------
if [ ! -f "$root/build/libcat89.a" ]; then
    printf 'ARCH SKIP: build/libcat89.a absent (run just build first)\n'
else
    bad=0
    nm -g --defined-only "$root/build/libcat89.a" \
        | awk '$2 ~ /^[TtDdBb]$/ {print $3}' \
        | sort -u \
        | while read -r sym; do
            case "$sym" in
                cat89_*) : ;;
                *) printf 'ARCH FAIL: non-cat89_ exported symbol <%s>\n' "$sym"; exit 1 ;;
            esac
        done || bad=1
    # every cat89_ symbol must be declared in a public header or internal.h
    nm -g --defined-only "$root/build/libcat89.a" \
        | awk '$2 ~ /^[TtDdBb]$/ {print $3}' \
        | sort -u \
        | grep '^cat89_' \
        | while read -r sym; do
            if ! grep -rq "\b$sym\b" "$inc" "$src/cat89_internal.h" 2>/dev/null; then
                printf 'ARCH FAIL: exported %s has no declaration in a public header\n' "$sym"
                exit 1
            fi
        done || bad=1
    if [ "$bad" -eq 0 ]; then
        echo "ARCH PASS: exported symbols are cat89_-prefixed and header-declared"
    else
        exit 1
    fi
fi

# ---- informational: exposed struct layouts --------------------------------
printf 'ARCH NOTE: public header struct references (review for exposure):\n'
grep -rn 'struct cat89_' "$inc" | sed 's/^/  /' || true

echo "ARCH PASS: architecture gate clean"
