# Module boundaries and decisions

## Decision: primitive-only V1 (spec §51 + §10)

The generic AST adapter (`hm_infer`, `hm_expr_ops`), rollback/checkpoints,
rigid variables, and the constructor registry are deferred. Rationale
(spec §59, §57): the §51 surface is a complete, self-consistent base on which
a host implements Algorithm W; keeping it small makes the green-clean core
easy to verify and audit. Adding the adapter later does not alter §51.

## Decision: split modules with a shared internal header

Each concern is one `.c` under `src/` (spec §58): ctx, type, unify, scheme,
env, ftv, error, print. Struct layouts and internal helper prototypes live in
`src/hm_internal.h`, included by every module. Public opaque types live in
`include/hm.h`. Green checks `src/*.c` (`project_roots: src`); the public
header is parsed within each owned TU and must stay strict-C89 clean under
the baseline flags.

## Data model

Internal (in `hm_internal.h`):

```c
struct hm_type {
    int kind;                 /* HM_TYPE_VAR or HM_TYPE_CON */
    union {
        struct { unsigned long id; struct hm_type *link; } var;
        struct { char *name; size_t arity; struct hm_type **args; } con;
    } u;
};

struct hm_scheme { size_t count; hm_type **vars; hm_type *body; };
struct hm_binding { char *name; hm_scheme *scheme; struct hm_binding *next; };
struct hm_env { struct hm_env *parent; struct hm_binding *bindings; };
```

`hm_ctx` holds the allocator, a `next_var_id` counter, the head of the
allocation list, and the current origin pointer. `link == NULL` means an
unbound variable. Constructor nodes and schemes are immutable after
construction.

## Allocation list

Each allocation records a header `struct { struct hm_blk *next; }` followed by
its payload; the payload is returned to callers. `hm_ctx_destroy` walks the
headers and frees each through the context allocator. This is C89-clean (no
flexible array members, no compound literals) and needs no typed destructors.
