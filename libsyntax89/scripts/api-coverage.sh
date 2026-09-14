#!/bin/sh -eu
set -eu
# api-coverage.sh - every public syntax89_* function is named by a test.
#
# The header is the API contract; this check proves only that a test names
# each public function, not that the tests are complete.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"

names=$(grep -o 'syntax89_[a-z_]*(' "$root/include/syntax89.h" | sed 's/($//' |
    sort -u)

for name in syntax89_init syntax89_destroy syntax89_add_node \
    syntax89_add_child syntax89_set_root syntax89_freeze syntax89_validate \
    syntax89_node syntax89_child_at syntax89_children_next \
    syntax89_walk_nodes_pre syntax89_walk_edges_post; do
    if ! printf '%s\n' "$names" | grep -qx -- "$name"; then
        echo "api-coverage: header is missing $name" >&2
        exit 1
    fi
done

printf '%s\n' "$names" | while read -r name; do
    case "$name" in
        *_fn)
            continue
            ;;
    esac
    if ! grep -rq -- "$name" "$root/test"; then
        echo "api-coverage: no test names $name" >&2
        exit 1
    fi
done

echo "api-coverage: clean"
