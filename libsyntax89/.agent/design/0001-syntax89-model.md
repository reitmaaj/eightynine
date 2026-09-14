# 0001 — libsyntax89 data model and invariants

## Public types

```c
typedef unsigned long syntax89_id;
typedef unsigned long syntax89_kind;
typedef unsigned long syntax89_role;
typedef unsigned long syntax89_source_id;
typedef unsigned long syntax89_offset;

#define SYNTAX89_ID_NONE ((syntax89_id)0)

typedef struct syntax89_span
{
    syntax89_source_id source;
    syntax89_offset begin;
    syntax89_offset end;
} syntax89_span;

typedef struct syntax89_node_info
{
    syntax89_kind kind;
    syntax89_span span;
} syntax89_node_info;

typedef struct syntax89_allocator
{
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t size);
    void (*free)(void *ctx, void *ptr);
} syntax89_allocator;
```

`syntax89_graph` is caller-owned. Its fields (allocator copy, node store,
counts, root, state, test limits) are private implementation state and must
not be read or written by clients. The layout is frozen once released.

`syntax89_child_iter` holds a borrowed graph pointer, a parent id, and an
index; it allocates nothing.

`syntax89_validation` carries `error`, `node`, and `related`.

## Internal storage

```c
struct syntax89_edge
{
    syntax89_role role;
    syntax89_id child;
};

struct syntax89_node
{
    syntax89_kind kind;
    syntax89_span span;
    struct syntax89_edge *edges;   /* insertion-ordered vector */
    unsigned long edge_count;
    unsigned long edge_capacity;
};

struct syntax89_frame
{
    syntax89_id id;       /* explicit depth-first frame */
    unsigned long index;
};

struct syntax89_scratch
{
    unsigned char *color;        /* one per node */
    struct syntax89_frame *stack; /* one per node */
};
```

Node ids are dense: id `i` names `nodes[i - 1]`, so lookup is O(1). Density is
an implementation detail, not a public promise; only nonzero, stability, and
no reuse are promised.

Growth doubles capacities with checked `unsigned long` arithmetic; the initial
node capacity is 8 and the initial edge capacity is 4. A failed growth leaves
the old pointer, count, and capacity untouched.

## Module map

| File | Responsibility |
| --- | --- |
| `include/syntax89.h` | public types, status enum, function contracts |
| `src/syntax89_internal.h` | states, colors, internal structs, helper prototypes |
| `src/syntax89_mem.c` | checked arithmetic, allocator wrappers, store growth, scratch |
| `src/syntax89_graph.c` | init, destroy, state, counts, root, test hooks |
| `src/syntax89_build.c` | add_node, add_child, set_root |
| `src/syntax89_query.c` | node and child inspection, role helpers |
| `src/syntax89_iter.c` | allocation-free child iteration |
| `src/syntax89_validate.c` | iterative DFS validation and freeze |
| `src/syntax89_walk.c` | node-once and occurrence traversal |

## Normative invariants

Always true in a valid graph:

- I1 every issued id names exactly one node; `SYNTAX89_ID_NONE` names none;
- I2 every edge endpoint names an existing node of the same graph;
- I3 `child_at(parent, i)` is the i-th appended edge of parent;
- I4 `edge_count` equals the sum of `child_count` over all nodes;
- I5 every span satisfies `begin <= end`; kind and span never change;
- I6 raw storage is coherent: counts never exceed capacities and nonempty
  vectors have non-NULL storage;
- I7 ids are nonzero, stable, and never reused;
- I8 queries and child iteration allocate nothing and mutate nothing;
- I9 every failed operation leaves state, root, counts, node metadata, and
  edge order unchanged;
- I10 state moves only UNINITIALIZED -> BUILDING -> FROZEN; `destroy` is
  allowed from any state;
- I11 a FROZEN graph is a single-root DAG: all nodes reachable from the root,
  no cycles, and the root has no incoming edge;
- I12 `validate(g) == SYNTAX89_OK` iff `freeze(g)` is `SYNTAX89_OK` or
  `SYNTAX89_ENOMEM`.

Freeze-specific invariants:

- F1 root is set and names a node;
- F2 every node is reachable from the root;
- F3 the structural graph is acyclic (including self-loops);
- F4 the root has no incoming edge (implied by F2 and F3);
- F5 freeze changes no id, kind, span, or edge order;
- F6 a FROZEN graph rejects every structural mutator with `SYNTAX89_ESTATE`;
- F7 freezing twice is an idempotent `SYNTAX89_OK`.

## Green constraints

The source is strict C89 and strict C23, warning-clean under GCC and Clang,
passes the `green` clang-tidy semantic checks, and uses the canonical Allman
format. In particular: no `&&`, `||`, `?:`, or comma operator in expression
position; assignments only as complete statements or `for` clauses; effectful
calls only as complete transitions; every controlled body braced; one object
per declaration; nested blocks are thin and delegate computation to workers;
pure helpers are listed in `green.yaml`.
