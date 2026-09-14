# libsyntax89 specification (v1)

`libsyntax89` is a small, language-agnostic C89 library for representing and
transforming abstract syntax without owning parsing, name resolution, typing,
evaluation, or language-specific node definitions.

Its responsibility is narrow:

> store typed syntax nodes, typed ordered structural edges, and source
> provenance, and provide generic construction, validation, freezing,
> traversal, and iteration.

Everything semantic stays outside.

## 1. Core model

A typed, ordered syntax graph:

```text
syntax
 ├─ nodes
 │   ├─ stable id
 │   ├─ client-defined kind
 │   └─ optional source span
 └─ edges
     ├─ client-defined role
     ├─ source node
     ├─ destination node
     └─ insertion order
```

The library never interprets kind or role values. A call expression is:

```text
CALL
 ├─ CALLEE ──> NAME
 ├─ ARG[0] ──> INT
 └─ ARG[1] ──> NAME
```

## 2. Structural edges only

Structural edges describe syntax composition and form a DAG. Non-owning
cross-links (resolved symbols, inferred types, declaration targets) are not
part of v1. Semantic passes keep side tables keyed by node id:

```c
node_id -> symbol
node_id -> inferred type
node_id -> declaration node
```

This keeps the syntax graph genuinely syntactic.

## 3. Stable node ids

```c
typedef unsigned long syntax89_id;
typedef unsigned long syntax89_kind;
typedef unsigned long syntax89_role;
```

`SYNTAX89_ID_NONE` (`0`) never names a node. Ids are stable for the graph
lifetime and are never reused. Internal storage may move freely.

## 4. Source spans

```c
typedef unsigned long syntax89_source_id;
typedef unsigned long syntax89_offset;

typedef struct syntax89_span
{
    syntax89_source_id source;
    syntax89_offset begin;
    syntax89_offset end;
} syntax89_span;
```

Semantics: half-open byte range `[begin, end)`. Unknown span: all three
fields zero. The library never owns or reads source text.

## 5. Nodes

```c
typedef struct syntax89_node_info
{
    syntax89_kind kind;
    syntax89_span span;
} syntax89_node_info;
```

No generic value union, literal payloads, identifier strings, symbol ids,
types, comments, or arbitrary properties. Language frontends keep those in
external tables keyed by `syntax89_id`.

## 6. Edges

An edge is `(role, child)` inside a parent's insertion-ordered edge vector.
Ordering follows insertion order; there is no separate ordinal.

Operations: `syntax89_child_count`, `syntax89_child_at`,
`syntax89_child_count_role`, `syntax89_child_at_role`, and the allocation-free
`syntax89_child_iter` begin/next pair.

## 7. Ownership

One `syntax89_graph` owns all nodes and structural edges through the allocator
copied at `syntax89_init`. A node belongs to exactly one graph. Edges may only
join nodes of the same graph. Clients own source buffers and semantic tables.

## 8. Construction API

```c
syntax89_status syntax89_init(syntax89_graph *g,
                              const syntax89_allocator *alloc);
void syntax89_destroy(syntax89_graph *g);
syntax89_status syntax89_add_node(syntax89_graph *g, syntax89_kind kind,
                                  syntax89_span span, syntax89_id *out);
syntax89_status syntax89_add_child(syntax89_graph *g, syntax89_id parent,
                                   syntax89_role role, syntax89_id child);
syntax89_status syntax89_set_root(syntax89_graph *g, syntax89_id root);
```

Exactly one distinguished root exists. The root need not mean "program"; a
fragment graph simply sets the fragment node as root.

## 9. Structural invariants

For every valid graph:

1. node id `0` never names a node;
2. every edge endpoint names a node of the same graph;
3. the root names an existing node;
4. every node except the root has at least one incoming structural edge;
5. the root has zero incoming structural edges;
6. structural edges contain no cycles;
7. child order remains deterministic (insertion order);
8. node ids never change;
9. ids are never reused (no deletion exists in v1);
10. source spans satisfy `begin <= end`.

Shared children are allowed, so the structure is a DAG rather than a tree.

## 10. Mutation policy

BUILDING permits add node, add child, and set root. FROZEN permits query,
traverse, validate, and rewrite into another graph. `syntax89_freeze` and
`syntax89_is_frozen` expose the state. After freezing, structural mutation is
rejected with `SYNTAX89_ESTATE`.

## 11. Rewriting model

Functional graph rewriting: an input frozen graph passes through a transform
that builds a new graph. Generic rewrite machinery is deliberately postponed;
v1 provides construction, validation, and traversal.

## 12. Traversal

Two independent distinctions:

- node-once (`syntax89_walk_nodes_pre`, `syntax89_walk_nodes_post`) visits
  each node at most once;
- occurrence (`syntax89_walk_edges_pre`, `syntax89_walk_edges_post`) visits
  each structural occurrence, revisiting shared nodes.

All walks are iterative, deterministic, follow insertion order, and return
`SYNTAX89_ECYCLE` on a back edge.

## 13. Iterators

`syntax89_children_begin`/`syntax89_children_next` yield each edge in
insertion order, return `SYNTAX89_END` at exhaustion, and allocate nothing.

## 14. Queries

`syntax89_node`, `syntax89_child_count`, `syntax89_child_at`,
`syntax89_child_count_role`, `syntax89_child_at_role`, `syntax89_root`,
`syntax89_node_count`, `syntax89_edge_count`, `syntax89_is_frozen`.

## 15. Validation

`syntax89_validate` is a first-class concern. It checks invalid ids, invalid
root, cycles, unreachable nodes, broken spans, and corrupt child ranges.
Language-specific validation is layered on top.

## 16. Schemas

Not in core v1. A declarative schema layer belongs in a separate library
(`libsyntaxschema89`) or a later version.

## 17. Annotations

Not in v1. No generic attribute API; side tables are the recommended
mechanism.

## 18. Deletion

Omitted in v1. Construct, freeze, transform into a replacement graph, destroy
the old graph.

## 19. Parent lookup

No stored parent pointers. Reverse traversal, if needed, uses an external
index.

## 20. Root and fragments

One root; any node may be the root of a fragment graph.

## 21. Allocation

```c
typedef struct syntax89_allocator
{
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t size);
    void (*free)(void *ctx, void *ptr);
} syntax89_allocator;
```

A NULL allocator selects the C library malloc/realloc/free. Queries and child
iteration allocate nothing. `validate`, `freeze`, and walks allocate O(V)
scratch and return `SYNTAX89_ENOMEM` on failure without observable mutation.

## 22. Error model

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

No global error state. Every mutating operation is unchanged-on-failure.

## 23. Cycle handling

Cycles are permitted during BUILDING and rejected at `syntax89_freeze`.
`add_child` stays cheap. If freeze detects a cycle, the graph stays mutable
and unchanged.

## 24. State machine

```text
UNINITIALIZED --init--> BUILDING --freeze--> FROZEN --destroy--> end
```

| Operation | BUILDING | FROZEN |
| --- | ---: | ---: |
| add node / add edge / set root | yes | no (`ESTATE`) |
| inspect / iterate / traverse | yes | yes |
| validate | yes | yes |
| freeze | yes | idempotent `OK` |
| rewrite source | build a new graph | yes |

## 25. Internal storage

Nodes array plus one insertion-ordered edge vector per node. Growth doubles
with checked arithmetic; failure leaves the store unchanged. Freeze is
metadata-only, so it is atomic by construction.

## 26. Serialization

Not in v1. Stable ids make a future format possible.

## 27. Layering

```text
libu89 / liblex89 -> parser -> libsyntax89 -> resolver / typer / formatter
```

`libsyntax89` depends only on C89 libc.

## 28. Example

```c
syntax89_graph g;
syntax89_id call;

syntax89_init(&g, &alloc);
syntax89_add_node(&g, MY_CALL, call_span, &call);
syntax89_add_node(&g, MY_NAME, name_span, &fn);
syntax89_add_child(&g, call, MY_CALLEE, fn);
syntax89_set_root(&g, call);
syntax89_freeze(&g);
```

## 29. v1 public surface

Lifecycle: `init`, `destroy`.
Construction: `add_node`, `add_child`, `set_root`.
State: `freeze`, `is_frozen`, `root`, `node_count`, `edge_count`.
Inspection: `node`, `child_count`, `child_at`, `child_count_role`,
`child_at_role`, `children_begin`, `children_next`.
Traversal: `walk_nodes_pre`, `walk_nodes_post`, `walk_edges_pre`,
`walk_edges_post`.
Validation: `validate`.

## 30. Explicit non-goals

Lexing, parsing, grammar definitions, parser generation, token storage, source
text ownership, symbol tables, scopes, name resolution, type systems,
evaluation, interpretation, bytecode, generic IR, arbitrary graph databases,
generic annotations, schemas, serialization, pretty printing, syntax
formatting, diffing, incremental parsing, deletion, and in-place rewrite.

## 31. Locked v1 decisions

| Decision | Choice |
|---|---|
| Graph handle | caller-owned public struct, private fields, ABI frozen |
| Traversal | node-once and occurrence walks, all iterative |
| Validation scratch | O(V) allocator scratch; may return `ENOMEM`; never mutates |
| Allocator default | `NULL` selects malloc/realloc/free |
| `init` | lazy; allocates nothing; first `add_node` allocates |
| `freeze` twice | idempotent `SYNTAX89_OK` |
| `destroy` | frees and zeroes; a second destroy is a harmless no-op |
| Ids | nonzero, stable, never reused; density is not promised |
| kind/role `0` | ordinary values; only `SYNTAX89_ID_NONE` is reserved |
| Malformed spans | rejected immediately by `add_node` with `EINVAL` |
| Error precedence | `EINVAL` > `ESTATE` > root `EGRAPH` > `ECYCLE` > reachability `EGRAPH` |

## 32. Design criterion

```text
node  = (id, kind, span)
edge  = (parent, role, child, order)
graph = (nodes, edges, root)
```

A small C89 library for constructing, validating, freezing, traversing, and
transforming typed ordered syntax DAGs.
