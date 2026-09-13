# Foundations

## Type graph

A type is either a variable or a constructor application:

```text
type := variable | constructor(name, arguments...)
```

Function types are ordinary constructor applications, conventionally
`->(argument, result)`.

## Variable representation

A variable carries a stable `unsigned long` id and a mutable link. An unbound
variable has `link == NULL`; after unification a variable may point to another
type node in the same context. All operations prune (follow links) before
inspecting. Linking never creates a cycle (occurs check guards binding).

## Context ownership

All objects allocated through one `hm_ctx` remain valid until
`hm_ctx_destroy`. The context owns an allocation list; every allocation is a
header node followed by its payload, and destroy walks the list freeing each
header via the context allocator's free callback. Constructors copy their
names into the context. Environments and errors copy names too. No public
per-object destructor exists.

## Module boundaries

- `hm_ctx` — context lifecycle, allocator, allocation list, variable-id
  counter, origin pointer.
- `hm_type` — node creation (var, con, const, app1, app2, fun), kind/id
  access, prune, structural equality.
- `hm_unify` — unification and the occurs check.
- `hm_scheme` — monomorphic/quantified schemes, instantiate, generalize.
- `hm_env` — lexical environments (linked list, child scoping, shadowing).
- `hm_ftv` — free-variable set routines backing generalization and env ftv.
- `hm_error` — error node construction and accessors.
- `hm_print` — deterministic type/scheme rendering.

`src/hm_internal.h` shares the struct definitions and internal prototypes
(`hm_i_*`) across modules.

## Key types

Public header `include/hm.h` declares opaque types `hm_ctx`, `hm_type`,
`hm_scheme`, `hm_env`, `hm_error`, enums `hm_status`, `hm_type_kind`,
`hm_error_kind`, the allocator and write callbacks, and the §51 primitive
functions plus §10 convenience helpers.

## Green-compliance mapping

Link writes are store transitions: each must be a standalone statement.
Allocator/name-copy calls are effects: each must be a result-binding
assignment, never nested. No `&&`/`||`/`?:`/comma in expression position
(use nested ifs). Computation inside `if`/loop bodies is factored into short
workers (green-flat). Truly pure helpers are marked `GREEN_PURE`. `NULL` is
the null spelling. Allman clang-format, four-space indent, 80 columns.
