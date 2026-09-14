# 0002 — libsyntax89 lifecycle, failure, and validation

## State machine

```text
UNINITIALIZED
      | init
      v
   BUILDING -------- freeze ------> FROZEN
      |                              |
      +---------- destroy -----------+
                     |
                     v
                UNINITIALIZED
```

| Operation | UNINITIALIZED | BUILDING | FROZEN |
| --- | ---: | ---: | ---: |
| `add_node` | `ESTATE` | yes | `ESTATE` |
| `add_child` | `ESTATE` | yes | `ESTATE` |
| `set_root` | `ESTATE` | yes | `ESTATE` |
| `node` / `child_at` / iterators | `ESTATE` | yes | yes |
| `child_count*` / `node_count` / `edge_count` | 0 | yes | yes |
| `validate` | `ESTATE` | yes | yes |
| `freeze` | `ESTATE` | yes | idempotent `OK` |
| `destroy` | no-op | yes | yes |

`init` zeroes the graph, copies the allocator (or installs the libc default),
and enters BUILDING. It allocates nothing. `destroy` frees every node edge
vector and the node store, then zeroes the struct; a second `destroy` is a
harmless no-op.

## Error model

```c
typedef enum syntax89_status
{
    SYNTAX89_OK = 0,
    SYNTAX89_END = 1,
    SYNTAX89_EINVAL = -1,
    SYNTAX89_ENOMEM = -2,
    SYNTAX89_ENODE = -3,
    SYNTAX89_ESTATE = -4,
    SYNTAX89_ECYCLE = -5,
    SYNTAX89_EGRAPH = -6,
    SYNTAX89_EOVERFLOW = -7
} syntax89_status;
```

`SYNTAX89_END` is positive because iterator and traversal exhaustion are
ordinary outcomes, not errors. No global error state exists.

## Failure atomicity

Every operation is all-or-nothing:

- `add_node` validates arguments, checks limits, grows the node store, and
  only then appends; `*out` is written only on success.
- `add_child` validates both endpoints and limits, grows the parent edge
  vector, and only then writes the edge and increments the counters.
- `freeze` validates before flipping the state; a failed validation leaves the
  graph BUILDING and byte-for-byte logically unchanged.
- `validate` and walks allocate scratch, free it on every path, and never
  mutate observable state.

The test suite proves this with a snapshot harness around every mutator and a
fault allocator that fails the Nth allocation.

## Validation algorithm

```text
1. reject NULL, UNINITIALIZED, and missing/unknown root (EINVAL/ESTATE/EGRAPH)
2. allocate color and frame scratch (ENOMEM; graph unchanged)
3. iterative depth-first search from the root with white/gray/black colors
4. an edge to a gray node is a cycle (ECYCLE; node = cycle entry,
   related = edge source)
5. after the search, any non-black node is unreachable (EGRAPH;
   node = lowest unreached id)
6. otherwise OK
```

The search is iterative: an explicit frame stack bounds C stack usage by
nothing (no recursion). The root stays gray for the whole search, so any
incoming edge to the root is reported as a cycle, which is exactly what
invariant F4 requires.

Precedence is deterministic and documented: `EINVAL` > `ESTATE` >
root `EGRAPH` > `ECYCLE` > reachability `EGRAPH`; `ENOMEM` may interrupt any
point before the structural result is final.

## Traversal

`walk_nodes_pre`/`walk_nodes_post` use the same frame engine with a visited
color: a node is visited at most once, on discovery for preorder and on finish
for postorder. `walk_edges_pre`/`walk_edges_post` re-enter black nodes so each
structural occurrence is visited; a gray node means a cycle. Callbacks return
`SYNTAX89_OK` to continue, `SYNTAX89_END` to stop successfully, or a negative
status that propagates verbatim; any other value aborts with `SYNTAX89_EINVAL`.

## Integer safety

Counts and byte sizes are checked before use:

- `syntax89__size_add` rejects wraparound;
- `syntax89__size_mul` rejects multiplication overflow;
- `add_node`/`add_child` reject a count at `ULONG_MAX` and honor the test-only
  limits used to force overflow deterministically;
- growth computes capacities and byte sizes with the checked helpers, so no
  wrapped allocation is ever attempted.

## Test-only hooks

`syntax89__set_limit_nodes` and `syntax89__set_limit_edges` are internal
symbols declared in `src/syntax89_internal.h` and exported for white-box
tests. They are not part of the public API and production code leaves the
limits at zero.
