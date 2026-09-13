# libhm89 — generic Hindley–Milner type inference for strict C89

`libhm89` is a small, embeddable, dependency-free implementation of
rank-1 Hindley–Milner type inference written in strict ISO C89. It ships the
**primitive generic API** of its specification (`spec/hm-spec.md`, §51 plus
the §10 convenience helpers) and nothing else: no AST, no source language, no
built-in types. A host supplies those and uses the primitives to implement
its own inference.

The library is **green-compliant**: it compiles warning-clean as strict C89
and strict C23 under both GCC and Clang, passes the green semantic checks,
and uses one canonical Allman format (verified by the sibling
[`green`](../../green) toolchain).

```text
                    host language / AST
                          |
                          v
   generic HM API   (this library)
   variables · constructors · unification
   occurs check · schemes · generalization
   instantiation · environments · errors
                          |
                          v
                   C89 runtime (stdlib only)
```

---

## Table of contents

- [Concept](#concept)
- [Build and test](#build-and-test)
- [A minimal example](#a-minimal-example)
- [Memory and ownership](#memory-and-ownership)
- [API reference](#api-reference)
  - [Opaque types](#opaque-types)
  - [Status values](#status-values)
  - [Allocator](#allocator)
  - [Context lifecycle](#context-lifecycle)
  - [Type representation](#type-representation)
  - [Unification](#unification)
  - [Type schemes](#type-schemes)
  - [Environments](#environments)
  - [Errors](#errors)
  - [Rendering](#rendering)
- [Types and variables in plain words](#types-and-variables-in-plain-words)
- [Writing your own inference](#writing-your-own-inference)
- [Conventions and constraints](#conventions-and-constraints)
- [Scope and non-goals](#scope-and-non-goals)

---

## Concept

Hindley–Milner (HM) infers the most general type of an expression and rejects
ill-typed ones. This library models the *type level* only:

```text
type :=  variable                        -- 'a, 'b, ...
      |  constructor(name, argument, ...) -- Int, List('a), ->(arg, result)
scheme := forall 'a 'b ... . type        -- polymorphic type
```

The type variables are *mutable*: unification destructively links one
variable to another type. The library provides the small set of operations
that Algorithm W needs — fresh variables, constructor application,
unification with occurs checking, generalization, instantiation, and lexical
environments — and leaves the expression syntax, the traversal order, and the
language semantics to the host.

For the full normative requirements, conformance tests, and recommended
internal design, see [`spec/hm-spec.md`](spec/hm-spec.md) and the documents
under `.agent/`.

---

## Build and test

The project uses `just` for every action (a `Makefile` delegates to it).

```sh
just build     # compile build/libhm89.a
just test      # smoke test first, then the unit suite
just green     # generate GCC/Clang compile DBs and run the seven-cell green matrix
just check     # = green
just format    # canonicalize formatting (clang-format)
just lint      # shellcheck + clang-format dry-run
just doctor    # verify external tools
```

Compile a program against the built archive:

```sh
cc -std=c89 -pedantic-errors -Wall -Wextra -Iinclude \
   -o prog prog.c build/libhm89.a
```

or, if you prefer to vendor the sources, compile the eight `src/hm_*.c`
translation units together with your program.

There is no `main()` and no runtime dependency beyond the C89 standard
library; the archive is a pure library.

---

## A minimal example

The example below builds the polymorphic identity `forall a. a -> a`,
instantiates it twice, and applies each copy to a different concrete type —
the essence of let-polymorphism.

```c
#include <stdio.h>
#include <string.h>

#include <hm.h>

int main(void)
{
    hm_ctx *ctx;
    hm_type *a;          /* a type variable          */
    hm_type *identity;   /* a -> a                    */
    hm_scheme *poly;     /* forall a. a -> a          */
    hm_type *use1;       /* instantiation 1           */
    hm_type *use2;       /* instantiation 2           */
    hm_status st;

    ctx = hm_ctx_new(NULL);          /* default malloc/free */
    a = hm_type_var(ctx);            /* fresh variable      */
    identity = hm_type_fun(ctx, a, a);           /* a -> a  */
    poly = hm_generalize(ctx, hm_env_new(ctx), identity);

    use1 = hm_instantiate(ctx, poly);
    use2 = hm_instantiate(ctx, poly);

    /* use1 must accept Int: unify with Int -> 'r */
    st = hm_unify(ctx, use1,
                  hm_type_fun(ctx, hm_type_const(ctx, "Int"),
                              hm_type_var(ctx)), NULL);
    if (st != HM_OK)
        return 1;

    /* use2 must accept Bool: unify with Bool -> 'r */
    st = hm_unify(ctx, use2,
                  hm_type_fun(ctx, hm_type_const(ctx, "Bool"),
                              hm_type_var(ctx)), NULL);
    if (st != HM_OK)
        return 2;

    hm_ctx_destroy(ctx);
    printf("identity is polymorphic\n");
    return 0;
}
```

Save as `demo.c` and build:

```sh
cc -std=c89 -pedantic-errors -Wall -Wextra -Iinclude demo.c build/libhm89.a -o demo
./demo
```

> `hm_type_var(ctx)` appears both to create the free variable and (via
> `hm_type_var` in the argument position) as the fresh result variable of an
> application. Each call returns a brand-new variable.

---

## Memory and ownership

All objects are **context-scoped**. Every type, scheme, environment, and
error you create through one `hm_ctx` stays valid until you destroy that
context; there are no individual destructors.

```c
hm_ctx *ctx = hm_ctx_new(NULL);
/* ... use hm_type_*, hm_scheme_*, hm_env_*, hm_unify ... */
hm_ctx_destroy(ctx);   /* frees everything the context ever allocated */
```

Rules:

- **Never** use an object across two different contexts. A context is not
  thread-safe for concurrent inference; separate contexts may run
  concurrently.
- Passing `NULL` as the allocator selects `malloc`/`free`. To inject a
  custom allocator, provide an `hm_allocator` (see below).
- Constructor names and environment binding names are copied by the library;
  you may pass transient strings.
- Allocation of zero bytes is handled internally and never relies on
  `malloc(0)` behaviour.
- Because unification is destructive, a **failed** unification can leave
  earlier successful bindings in place. Either discard the context after a
  failure or treat the current inference branch as invalidated.

---

## API reference

Everything is declared in `include/hm.h`. Public identifiers begin with
`hm_`; public constants with `HM_`.

### Opaque types

```c
typedef struct hm_ctx hm_ctx;
typedef struct hm_type hm_type;
typedef struct hm_scheme hm_scheme;
typedef struct hm_env hm_env;
typedef struct hm_error hm_error;
```

All five are opaque. `hm_ctx` owns memory and inference state. `hm_type` is a
type node. `hm_scheme` is a (possibly quantified) type. `hm_env` is a lexical
name→scheme mapping. `hm_error` is a structured error record.

### Status values

`hm_status` is returned by operations that can fail.

| Value | Meaning |
| --- | --- |
| `HM_OK` | success |
| `HM_ERROR_NOMEM` | allocation failed |
| `HM_ERROR_MISMATCH` | two incompatible constructor types |
| `HM_ERROR_OCCURS` | occurs-check failure (would form an infinite type) |
| `HM_ERROR_UNBOUND` | a name was not found (host-facing; see lookup) |
| `HM_ERROR_INVALID` | invalid argument |
| `HM_ERROR_INTERNAL` | internal invariant violated |

### Allocator

```c
typedef void *(*hm_alloc_fn)(void *userdata, size_t size);
typedef void (*hm_free_fn)(void *userdata, void *ptr);

typedef struct hm_allocator {
    hm_alloc_fn alloc;
    hm_free_fn free;
    void *userdata;
} hm_allocator;
```

An `hm_allocator` bundles the two callbacks plus an opaque userdata pointer.
`userdata` is passed back to both callbacks, so you can track allocations or
pull from a pool. `NULL` (as the whole allocator) means the default
`malloc`/`free`.

### Context lifecycle

```c
hm_ctx *hm_ctx_new(const hm_allocator *allocator);
void    hm_ctx_destroy(hm_ctx *ctx);
void    hm_ctx_set_origin(hm_ctx *ctx, void *origin);
```

- `hm_ctx_new` creates a context. It returns `NULL` if the first allocation
  fails. Pass `NULL` for the default allocator.
- `hm_ctx_destroy` reclaims every object the context ever allocated and then
  frees the context itself. It accepts `NULL`.
- `hm_ctx_set_origin` stores an opaque pointer that later errors will copy.
  The library never dereferences it; use it to attach an AST node or source
  range to failures.

### Type representation

```c
typedef enum hm_type_kind { HM_TYPE_VAR, HM_TYPE_CON } hm_type_kind;

hm_type_kind hm_type_kind_of(hm_type *type);
```

`hm_type_kind_of` reports whether a type is a variable or a constructor
application. It first follows variable links (see `hm_type_prune`).

**Variables**

```c
hm_type *hm_type_var(hm_ctx *ctx);
unsigned long hm_type_var_id(hm_type *type);
```

`hm_type_var` allocates a fresh, unbound variable and returns it. `NULL` on
no-memory. `hm_type_var_id` returns that variable's stable identifier, used
only for diagnostics and deterministic rendering; it carries no semantic
meaning. Calling it on a non-variable is invalid use and returns `0`.

**Constructors**

```c
hm_type *hm_type_con(hm_ctx *ctx, const char *name, size_t arity,
                     hm_type **args);
```

`hm_type_con` builds a constructor application `name(args[0], ..., args[n-1])`.
The name is copied into the context. With `arity == 0` you pass `args ==
NULL`, giving a constant like `Int`. With `arity == 2` and `name == "->"` you
get a function type. Two constructors with the same name **and** same arity
unify structurally; otherwise they are distinct.

Convenience helpers:

```c
hm_type *hm_type_const(hm_ctx *ctx, const char *name);              /* name/0        */
hm_type *hm_type_app1(hm_ctx *ctx, const char *name, hm_type *a);   /* name/1        */
hm_type *hm_type_app2(hm_ctx *ctx, const char *name, hm_type *a,
                      hm_type *b);                                  /* name/2        */
hm_type *hm_type_fun(hm_ctx *ctx, hm_type *arg, hm_type *result);   /* ->(arg,result)*/
```

`hm_type_fun` is exactly `hm_type_app2(ctx, "->", arg, result)`; no special
unification rule applies to `->`.

**Dereferencing**

```c
hm_type *hm_type_prune(hm_type *type);
```

Unbound variables may point at other variables that point at other types.
`hm_type_prune` follows the chain to an unbound variable or a constructor and
performs path compression along the way. You rarely need to call it yourself;
the library prunes internally. It returns the same node as its argument when
the argument is not a bound variable.

**Equality**

```c
int hm_type_equal(hm_type *a, hm_type *b);
```

Returns `1` when `a` and `b` are the same node or are structurally equal
after pruning; `0` otherwise. Two distinct unbound variables compare unequal.
This function only mutates unification state by path compression.

### Unification

```c
hm_status hm_unify(hm_ctx *ctx, hm_type *left, hm_type *right,
                   hm_error **error);
```

Unifies two types destructively:

1. prune both sides;
2. succeed if they are already the same node;
3. bind an unbound variable to the other side, but only after the
   occurs check passes;
4. compare constructor names and arities;
5. recurse over constructor arguments;
6. fail on incompatible constructors.

Returns `HM_OK`, `HM_ERROR_MISMATCH`, or `HM_ERROR_OCCURS`. If `error` is not
`NULL`, a failed call stores a populated `hm_error *` there (see
[Errors](#errors)); on success it stores `NULL`. Passing `NULL` skips error
construction.

```c
unify(->('a, Int), ->(Bool, 'b))  ==>  'a = Bool, 'b = Int     (HM_OK)
unify(Int, Bool)                  ==>  HM_ERROR_MISMATCH
unify('a, List('a))               ==>  HM_ERROR_OCCURS          ('a occurs in List('a))
```

### Type schemes

```c
hm_scheme *hm_scheme_mono(hm_ctx *ctx, hm_type *type);
hm_scheme *hm_scheme_new(hm_ctx *ctx, size_t quantified_count,
                         hm_type **quantified, hm_type *body);
size_t     hm_scheme_quantified_count(const hm_scheme *scheme);
hm_type   *hm_scheme_body(const hm_scheme *scheme);
```

A scheme is `forall v1 ... vn . body`.

- `hm_scheme_mono` builds a scheme with no quantified variables over `type`.
- `hm_scheme_new` builds an explicit scheme. Every entry of `quantified` must
  be an unbound variable and there must be no duplicates; otherwise the call
  returns `NULL` (rejection). `body` is the scheme's type.
- `hm_scheme_quantified_count` reports how many variables are quantified.
- `hm_scheme_body` returns the scheme's body type.

**Instantiation**

```c
hm_type *hm_instantiate(hm_ctx *ctx, const hm_scheme *scheme);
```

Instantiating `forall a. a -> a` yields a fresh copy `'x -> 'x` whose `'x` is
new; a second call yields an unrelated `'y -> 'y`. **Monomorphic** schemes
share their body instead: instantiating `'a -> 'a` returns the same `'a`,
which is exactly the sharing lambda-bound monomorphic variables need.
Returns the instantiated type, or `NULL` on no-memory.

**Generalization**

```c
hm_scheme *hm_generalize(hm_ctx *ctx, hm_env *env, hm_type *type);
```

Generalizes `type` relative to `env`, quantifying exactly the free variables
of `type` that are **not** free in `env`:

```c
generalize({},    'a -> 'a)  ==>  forall a. a -> a   (1 quantified)
generalize({x:'a}, 'a -> 'b) ==>  forall b. a -> b   (only b; a is monomorphic)
```

Returns the new scheme, or `NULL` on no-memory. Generalization is always
explicit — the core never generalizes on its own, so a host implementing a
value restriction decides when to call it.

### Environments

```c
hm_env *hm_env_new(hm_ctx *ctx);
hm_env *hm_env_child(hm_ctx *ctx, hm_env *parent);

hm_status hm_env_bind(hm_env *env, const char *name, hm_scheme *scheme);
hm_scheme *hm_env_lookup(hm_env *env, const char *name);
```

An environment maps names to schemes.

- `hm_env_new` creates an empty scope.
- `hm_env_child` creates a scope whose parent is `parent`. Lookup searches a
  scope first, then its parent, and so on.
- `hm_env_bind` adds a binding; the name is copied. A binding in a child
  shadows a same-named binding in an ancestor. Returns `HM_OK`,
  `HM_ERROR_NOMEM`, or `HM_ERROR_INVALID` (a `NULL` scheme).
- `hm_env_lookup` returns the visible scheme for `name`, or `NULL` when the
  name is unbound. (The host decides how to turn that `NULL` into an
  `HM_ERROR_UNBOUND` diagnostic.)

Typical use (Algorithm W):

```c
/* lambda: bind the parameter to a fresh monomorphic variable */
hm_type *param = hm_type_var(ctx);
hm_env  *inner = hm_env_child(ctx, env);
hm_env_bind(inner, "x", hm_scheme_mono(ctx, param));

/* let: generalize under the OUTER env, then bind in a child */
hm_scheme *poly = hm_generalize(ctx, env, value_type);
hm_env  *body_env = hm_env_child(ctx, env);
hm_env_bind(body_env, "id", poly);
```

### Errors

```c
typedef enum hm_error_kind {
    HM_ERR_NONE = 0, HM_ERR_NOMEM, HM_ERR_MISMATCH, HM_ERR_OCCURS,
    HM_ERR_UNBOUND, HM_ERR_INVALID, HM_ERR_INTERNAL
} hm_error_kind;

hm_error_kind hm_error_kind_of(const hm_error *error);
const char   *hm_error_message(const hm_error *error);
hm_type      *hm_error_left(const hm_error *error);
hm_type      *hm_error_right(const hm_error *error);
const char   *hm_error_name(const hm_error *error);
void         *hm_error_origin(const hm_error *error);
```

An `hm_error *` is returned through the `error` out-parameter of
`hm_unify`. Inspect it with the accessors only — never as a type.

- `hm_error_kind_of` — the kind (`HM_ERR_NONE` for `NULL`).
- `hm_error_message` — a human-readable message.
- For `HM_ERR_MISMATCH`, `hm_error_left` / `hm_error_right` name the two
  incompatible types.
- For `HM_ERR_OCCURS`, `hm_error_left` is the offending variable and
  `hm_error_right` the type that contains it.
- `hm_error_name` identifies a missing name (host-facing).
- `hm_error_origin` returns the pointer set by `hm_ctx_set_origin` when the
  error was produced.

Errors follow context lifetime; no free is needed.

### Rendering

```c
typedef int (*hm_write_fn)(void *userdata, const char *data, size_t size);

hm_status hm_type_write(hm_type *type, hm_write_fn write_fn, void *userdata);
hm_status hm_scheme_write(const hm_scheme *scheme, hm_write_fn write_fn,
                          void *userdata);
```

Renderers write deterministic debugging text through `write_fn`, which
receives chunks. `write_fn` returns non-zero to continue and zero to abort
(`hm_type_write` then returns `HM_ERROR_INTERNAL`; on an internal
allocation failure it returns `HM_ERROR_NOMEM`).

Display names are assigned by first encounter: `'a`, `'b`, ..., `'z`, `'a1`,
`'b1`, ... regardless of internal variable ids.

```c
Int
'a
List('a)
->('a, Bool)          /* canonical form, function types not prettified */
forall 'a 'b. ->('a, 'b)
```

A simple callback that accumulates into a buffer:

```c
#include <stddef.h>
struct buf { char *p; size_t n, cap; };

static int write_buf(void *u, const char *d, size_t s)
{
    struct buf *b = u;
    /* append s bytes of d to b->p ... */
    return 1;
}
```

---

## Types and variables in plain words

- **Constructor** — a named type applied to zero or more arguments. `Int` is
  a nullary constructor; `List(Int)` is a unary application; `->(a, b)` is a
  binary application used for function types. The library gives constructor
  names no meaning — `Int` is just the string `"Int"`.
- **Type variable** — a placeholder that can be unified with anything once.
  `'a` in `a -> a` can be `Int` or `Bool` or `List(Int)`, but whatever it
  becomes on the left it must also be on the right.
- **Scheme** — a type with universally quantified variables. `forall a. a ->
  a` says: each *use* may choose its own `'a`. That is what makes `id` usable
  at both `Int` and `Bool`.
- **Environment** — the set of in-scope names and their (possibly
  polymorphic) types.

HM decides how general an expression can be:

- Let-bound values get **generalized** (polymorphic), because their value is
  fixed once.
- Lambda-bound parameters stay **monomorphic**: `fun id -> Pair(id 1, id
  true)` fails, because `id` would have to be both `Int -> r` and
  `Bool -> r`.

---

## Writing your own inference

The library exposes the exact primitive set Algorithm W needs, so you do not
have to touch its internals:

| Algorithm W step | Library call |
| --- | --- |
| introduce a fresh metavariable | `hm_type_var(ctx)` |
| build a type `C(t1..tn)` | `hm_type_con(ctx, name, arity, args)` |
| look up a name's scheme | `hm_env_lookup(env, name)` |
| use a polymorphic name once | `hm_instantiate(ctx, scheme)` |
| constrain two types to agree | `hm_unify(ctx, t1, t2, &err)` |
| make a let-bound value polymorphic | `hm_generalize(ctx, env, type)` |
| extend the environment | `hm_env_bind(env, name, scheme)` |

See `spec/hm-spec.md` §19–20 for the canonical rules
(var/lambda/application/let) and `.agent/testing/0001-scenarios.md` for
worked examples such as `let id = fun x -> x in Pair(id 1, id true)`.

---

## Conventions and constraints

- **ISO C89 only.** No `//` comments, `inline`, `stdbool.h`, `stdint.h`,
  declarations after statements, designated initializers, compound literals,
  flexible array members, variadic macros, `_Bool`, or VLAs.
- **Green-compliant.** Strict C89 ∩ C23, GCC + Clang warning-clean, and the
  green semantic checks all pass (`just green`). Four-space indentation,
  Allman braces, 80 columns.
- **Namespacing.** Public functions are `hm_*`; public constants `HM_*`;
  private helpers `hm_i_*`.
- **Ownership.** Context-scoped; see [Memory and ownership](#memory-and-ownership).
- **Tests.** Smoke + unit programs under `test/` exercise the §56
  conformance matrix plus core-invariant guards and renderer checks.
- **Docs.** Concept, stories, design, BDD testing, and acceptance documents
  live under `.agent/`.

---

## Scope and non-goals

V1 intentionally does **not** include:

- any AST, parser, or source-language adapter (`hm_infer`, `hm_expr_ops`);
- rollback / checkpoints (`hm_ctx_mark` / `rollback` / `commit`);
- rigid / skolem variables and explicit annotation *checking*;
- a value restriction (the core never generalizes automatically);
- type classes / qualified types, records / row polymorphism, or effects;
- a constructor registry or interning;
- hard-coded built-in types.

Host languages that add mutable references or effects implement their own
value restriction on top of the explicit `hm_generalize`; hosts that want
records, classes, or effects build those layers on the exposed
construction and unification primitives.

---

## License

MIT. See [`LICENSE`](LICENSE). Normative specification:
[`spec/hm-spec.md`](spec/hm-spec.md).
