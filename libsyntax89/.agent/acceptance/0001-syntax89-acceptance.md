# 0001 — libsyntax89 acceptance

Behavior the library MUST exhibit and behavior it MUST reject. Each item maps
to at least one test in `test/`.

## Must exhibit

A1. `syntax89_init` yields a BUILDING graph with zero nodes, zero edges, and
    no root; no allocation occurs.

A2. `syntax89_add_node` appends a node, returns a fresh nonzero id, preserves
    kind and span exactly, and never changes earlier ids.

A3. `syntax89_add_child` appends after the parent's existing edges; absolute
    order and role-filtered relative order both follow insertion order.

A4. `syntax89_child_count`, `syntax89_child_at`, `syntax89_child_count_role`,
    `syntax89_child_at_role`, and the child iterator agree exactly.

A5. A shared child is one node with several incoming edges; node_count counts
    it once and edge_count counts every edge.

A6. `syntax89_freeze` succeeds exactly when a root exists, the root has no
    incoming edge, the graph is acyclic, and every node is reachable from the
    root.

A7. `syntax89_validate` reports the same defects without mutating anything and
    fills `syntax89_validation` with the offending node.

A8. Frozen graphs reject every structural mutator with `SYNTAX89_ESTATE`.

A9. `syntax89_walk_nodes_pre/post` visit each node once; `syntax89_walk_edges_
    pre/post` visit each structural occurrence; both are deterministic and
    follow insertion order.

A10. Callbacks can stop early with `SYNTAX89_END` or abort with a negative
     status that propagates verbatim.

A11. Every allocation failure returns `SYNTAX89_ENOMEM` and leaves the graph
     logically unchanged; `syntax89_destroy` returns every allocation.

A12. 100,000-deep and 100,000-wide graphs validate, freeze, walk, and destroy
     without C-stack recursion.

A13. Construction is deterministic: identical insertion sequences yield
     identical ids, order, traversal, and validation results.

## Must reject

R1. `syntax89_add_node` rejects `begin > end` with `SYNTAX89_EINVAL`.

R2. Unknown node ids and `SYNTAX89_ID_NONE` are rejected with
    `SYNTAX89_ENODE` wherever an id must name a node.

R3. Out-of-range child indexes are rejected with `SYNTAX89_EINVAL`; iterator
    exhaustion reports `SYNTAX89_END`.

R4. `syntax89_freeze` rejects a missing or unknown root with
    `SYNTAX89_EGRAPH`.

R5. `syntax89_freeze` rejects a reachable cycle with `SYNTAX89_ECYCLE` and an
    unreachable node with `SYNTAX89_EGRAPH`.

R6. Mutators on a FROZEN graph are rejected with `SYNTAX89_ESTATE` and change
    nothing.

R7. Every failed operation leaves state, root, counts, node metadata, and edge
    order byte-for-byte logically unchanged.

R8. Integer overflow in counts or byte sizes is rejected with
    `SYNTAX89_EOVERFLOW`; no wrapped allocation is attempted.

R9. NULL required pointers are rejected with `SYNTAX89_EINVAL`; documented
    NULL-tolerant queries return `SYNTAX89_ID_NONE` or 0.

R10. Walks reject an unknown root with `SYNTAX89_ENODE` and a cyclic region
     with `SYNTAX89_ECYCLE`.

R11. A callback returning a positive value other than `SYNTAX89_END` aborts
     with `SYNTAX89_EINVAL`.
