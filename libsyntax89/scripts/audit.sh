#!/bin/sh -eu
set -eu
# audit.sh - symbol audit for libsyntax89.
#
# 1. The archive may reference only malloc/realloc/free (the default
#    allocator); every other undefined name must be a documented internal
#    syntax89__ helper resolved inside the archive.
# 2. The exported symbol set must be exactly the public API plus the
#    documented internal helpers.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
lib="$root/build/libsyntax89.a"

if [ ! -e "$lib" ]; then
    echo "audit: $lib missing; run 'just build' first"
    exit 1
fi

external=$(nm -u "$lib" | awk 'NF >= 2 { print $NF }' |
    grep -v -E '^syntax89__[a-z_]+$' |
    grep -v -E '^(malloc|realloc|free)$' || true)
if [ -n "$external" ]; then
    echo "audit: unexpected undefined symbols:"
    printf '%s\n' "$external"
    exit 1
fi

expected="syntax89_add_child
syntax89_add_node
syntax89_child_at
syntax89_child_at_role
syntax89_child_count
syntax89_child_count_role
syntax89_children_begin
syntax89_children_next
syntax89_destroy
syntax89_edge_count
syntax89_freeze
syntax89_init
syntax89_is_frozen
syntax89_node
syntax89_node_count
syntax89_root
syntax89_set_root
syntax89_validate
syntax89_walk_edges_post
syntax89_walk_edges_pre
syntax89_walk_nodes_post
syntax89_walk_nodes_pre
syntax89__alloc
syntax89__free
syntax89__grow_edges
syntax89__grow_nodes
syntax89__node_at
syntax89__node_mut
syntax89__realloc
syntax89__scratch_free
syntax89__scratch_new
syntax89__set_limit_edges
syntax89__set_limit_nodes
syntax89__size_add
syntax89__size_mul
syntax89__span_ok"

actual=$(nm -g --defined-only "$lib" | awk 'NF >= 3 { print $NF }' | sort -u)
want=$(printf '%s\n' "$expected" | sort -u)

if [ "$actual" != "$want" ]; then
    echo "audit: exported symbol set mismatch"
    echo "--- actual ---"
    printf '%s\n' "$actual"
    echo "--- expected ---"
    printf '%s\n' "$want"
    exit 1
fi

echo "audit: ok"
