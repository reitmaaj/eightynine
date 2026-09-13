# Generic Hindley–Milner Type Inference Library for C89

## 1. Scope

This specification defines a small embeddable Hindley–Milner type inference library for ISO C89.

The library provides:

* type variables;
* type constructors;
* constructor application;
* function types as ordinary constructor applications;
* type schemes with universal quantification;
* lexical type environments;
* instantiation;
* generalization;
* unification;
* occurs checking;
* Algorithm W primitives;
* optional whole-expression inference through a host-supplied syntax adapter;
* structured error reporting;
* no dependency beyond the C89 standard library.

The library does not prescribe:

* a source language;
* an AST representation;
* primitive type names;
* syntax for function types;
* a particular allocator;
* algebraic data type declarations;
* type classes;
* subtyping;
* row polymorphism;
* effect systems;
* higher-rank polymorphism;
* GADTs.

The core models rank-1 Hindley–Milner polymorphism.

---

# 2. Design principles

## 2.1 Host-independent types

The HM implementation shall not depend on the host AST.

The library operates on its own type graph:

```text
type :=
    variable
  | constructor(name, arguments...)
```

Function types use an ordinary constructor, conventionally:

```text
->(argument, result)
```

Examples:

```text
Int
Bool
List(Int)
Pair(a, b)
->(Int, Bool)
->(a, a)
```

The core assigns no semantic meaning to constructor names.

---

## 2.2 C89 portability

The implementation shall compile as conforming C89.

The public API shall avoid:

* `//` comments;
* `inline`;
* `stdbool.h`;
* `stdint.h`;
* variable declarations after statements;
* designated initializers;
* compound literals;
* flexible array members;
* variadic macros;
* `_Bool`;
* VLAs.

Public integer identifiers shall use `unsigned long`.

---

## 2.3 Explicit ownership

The library shall expose context-scoped ownership.

All HM objects allocated through one `hm_ctx` remain valid until:

```c
hm_ctx_destroy(ctx);
```

Individual type nodes need no public destructor.

This policy simplifies graph sharing, substitutions, schemes, temporary inference structures, and error paths.

A host may supply custom allocation callbacks.

---

## 2.4 Mutable unification graph

Inference shall internally represent type variables using mutable links.

A variable starts unbound:

```text
VAR(id)
```

After unification it may point to another type:

```text
VAR(id) -> TYPE
```

Operations shall follow links before inspecting a type.

This representation corresponds to destructive substitutions and avoids repeatedly rebuilding complete type expressions.

---

# 3. Public names

All public identifiers shall start with:

```text
hm_
```

Public constants shall start with:

```text
HM_
```

The implementation may use identifiers beginning with:

```text
hm_i_
```

for private helpers.

---

# 4. Opaque public types

```c
typedef struct hm_ctx hm_ctx;
typedef struct hm_type hm_type;
typedef struct hm_scheme hm_scheme;
typedef struct hm_env hm_env;
typedef struct hm_error hm_error;
```

The public header shall not expose their representation.

---

# 5. Status values

```c
typedef enum hm_status {
    HM_OK = 0,
    HM_ERROR_NOMEM,
    HM_ERROR_MISMATCH,
    HM_ERROR_OCCURS,
    HM_ERROR_UNBOUND,
    HM_ERROR_INVALID,
    HM_ERROR_INTERNAL
} hm_status;
```

Functions returning `hm_status` shall return `HM_OK` on success.

---

# 6. Allocation interface

```c
typedef void *(*hm_alloc_fn)(void *userdata, size_t size);
typedef void (*hm_free_fn)(void *userdata, void *ptr);

typedef struct hm_allocator {
    hm_alloc_fn alloc;
    hm_free_fn free;
    void *userdata;
} hm_allocator;
```

Context construction:

```c
hm_ctx *hm_ctx_new(const hm_allocator *allocator);
void hm_ctx_destroy(hm_ctx *ctx);
```

Passing `NULL` selects `malloc` and `free`.

The implementation shall treat allocation of zero bytes internally without relying on implementation-specific `malloc(0)` behavior.

---

# 7. Type representation

A type belongs to one of two public categories:

```c
typedef enum hm_type_kind {
    HM_TYPE_VAR,
    HM_TYPE_CON
} hm_type_kind;
```

Public inspection:

```c
hm_type_kind hm_type_kind_of(hm_type *type);
```

`hm_type_kind_of()` shall inspect the representative after dereferencing variable links.

---

# 8. Type variables

Creation:

```c
hm_type *hm_type_var(hm_ctx *ctx);
```

Each call allocates a fresh type variable.

Variables shall carry stable identifiers:

```c
unsigned long hm_type_var_id(hm_type *type);
```

Calling `hm_type_var_id()` on a non-variable shall constitute invalid API use and may return `0`.

Variable identifiers exist for diagnostics and deterministic rendering only.

They carry no semantic meaning.

---

# 9. Type constructors

Constructor application:

```c
hm_type *hm_type_con(
    hm_ctx *ctx,
    const char *name,
    size_t arity,
    hm_type **args
);
```

Examples:

```c
hm_type_con(ctx, "Int", 0, NULL);
hm_type_con(ctx, "List", 1, args);
hm_type_con(ctx, "->", 2, args);
```

Constructor identity shall depend on:

```text
name + arity
```

Two constructors with equal names and equal arity unify structurally.

Two constructors with unequal names or unequal arities fail to unify.

The context shall copy constructor names.

---

# 10. Convenience constructor helpers

The library should provide:

```c
hm_type *hm_type_const(hm_ctx *ctx, const char *name);

hm_type *hm_type_app1(
    hm_ctx *ctx,
    const char *name,
    hm_type *a
);

hm_type *hm_type_app2(
    hm_ctx *ctx,
    const char *name,
    hm_type *a,
    hm_type *b
);

hm_type *hm_type_fun(
    hm_ctx *ctx,
    hm_type *arg,
    hm_type *result
);
```

`hm_type_fun()` shall construct:

```text
->(arg, result)
```

No special unification rule shall apply to `->`.

---

# 11. Dereferencing

```c
hm_type *hm_type_prune(hm_type *type);
```

`hm_type_prune()` shall follow variable links until reaching:

* an unbound variable; or
* a constructor.

It should perform path compression.

Equivalent conceptual operation:

```text
prune(VAR a -> VAR b -> Int)
    => Int

and rewrite:

a -> Int
b -> Int
```

Hosts normally need not call this function directly.

---

# 12. Unification

Primary operation:

```c
hm_status hm_unify(
    hm_ctx *ctx,
    hm_type *left,
    hm_type *right,
    hm_error **error
);
```

The algorithm shall:

1. prune both arguments;
2. succeed immediately when both arguments reference the same node;
3. bind an unbound variable to the other type after occurs checking;
4. compare constructor names and arities;
5. recursively unify corresponding constructor arguments;
6. fail on incompatible constructors.

Example:

```text
unify(->(a, Int), ->(Bool, b))

produces:

a = Bool
b = Int
```

---

# 13. Occurs check

Before binding variable `v` to type `t`, the implementation shall reject any binding where `v` occurs within `t`.

Example:

```text
a ~ List(a)
```

shall fail with:

```text
HM_ERROR_OCCURS
```

Likewise:

```text
a ~ ->(Int, a)
```

shall fail.

This requirement prevents infinite types.

---

# 14. Type schemes

A type scheme represents:

```text
forall v1 ... vn . T
```

Opaque type:

```c
hm_scheme
```

Construction:

```c
hm_scheme *hm_scheme_mono(
    hm_ctx *ctx,
    hm_type *type
);
```

creates a monomorphic scheme with zero quantified variables.

Generalization creates polymorphic schemes.

Inspection:

```c
size_t hm_scheme_quantified_count(const hm_scheme *scheme);
hm_type *hm_scheme_body(const hm_scheme *scheme);
```

The public API need not expose quantified variable nodes directly unless required for debugging.

---

# 15. Type environments

A type environment maps names to schemes.

Creation:

```c
hm_env *hm_env_new(hm_ctx *ctx);
```

Nested environment:

```c
hm_env *hm_env_child(
    hm_ctx *ctx,
    hm_env *parent
);
```

Binding:

```c
hm_status hm_env_bind(
    hm_env *env,
    const char *name,
    hm_scheme *scheme
);
```

Lookup:

```c
hm_scheme *hm_env_lookup(
    hm_env *env,
    const char *name
);
```

The environment shall copy names.

Lookup shall search:

```text
current scope
parent
parent's parent
...
```

Bindings in a child shadow bindings in ancestors.

Environment nodes follow context lifetime.

---

# 16. Free type variables

The implementation shall internally support:

```text
ftv(type)
ftv(scheme)
ftv(environment)
```

Definitions:

```text
ftv(VAR a) = {a}

ftv(C(T1 ... Tn))
    = ftv(T1) ∪ ... ∪ ftv(Tn)

ftv(forall a1 ... an . T)
    = ftv(T) - {a1 ... an}

ftv(environment)
    = union of ftv(scheme) for every visible binding
```

All types shall first undergo logical pruning during free-variable traversal.

---

# 17. Generalization

API:

```c
hm_scheme *hm_generalize(
    hm_ctx *ctx,
    hm_env *env,
    hm_type *type
);
```

Generalization shall quantify exactly:

```text
ftv(type) - ftv(env)
```

Example:

```text
env = {}

type = ->(a, a)

generalize(env, type)
    = forall a. a -> a
```

Example with an environment-bound variable:

```text
env:
    x : a

type:
    a -> b

generalize(env, type)
    = forall b. a -> b
```

The variable `a` shall remain monomorphic because it occurs free in the environment.

---

# 18. Instantiation

API:

```c
hm_type *hm_instantiate(
    hm_ctx *ctx,
    const hm_scheme *scheme
);
```

Instantiation shall replace every quantified variable with one fresh variable.

Non-quantified variables shall remain shared.

Given:

```text
forall a. a -> a
```

two calls shall yield logically distinct fresh types:

```text
b -> b
c -> c
```

with:

```text
b != c
```

---

# 19. Core Algorithm W operations

The library shall support Algorithm W independently from any host AST.

The minimal generic primitives consist of:

```text
fresh variable
constructor application
environment lookup
instantiate
unify
generalize
environment extension
```

A host language can implement inference using only these operations.

---

# 20. Canonical Algorithm W rules

For a lambda calculus with:

```text
e :=
    x
  | fun x -> e
  | e1 e2
  | let x = e1 in e2
```

the inference rules follow these procedures.

## 20.1 Variable

For:

```text
x
```

1. look up `x` in the environment;
2. fail with `HM_ERROR_UNBOUND` when absent;
3. instantiate its scheme;
4. return the instantiated type.

Conceptually:

```text
infer(env, x):
    s = lookup(env, x)
    return instantiate(s)
```

---

## 20.2 Lambda

For:

```text
fun x -> body
```

1. create fresh variable `a`;
2. create child environment;
3. bind:

```text
x : a
```

as a monomorphic scheme;
4. infer `body` as `b`;
5. return:

```text
a -> b
```

Conceptually:

```text
infer(env, fun x -> body):
    a = fresh()
    env2 = child(env)
    bind(env2, x, mono(a))
    b = infer(env2, body)
    return a -> b
```

---

## 20.3 Application

For:

```text
f x
```

1. infer `f` as type `tf`;
2. infer `x` as type `tx`;
3. create fresh result variable `tr`;
4. unify:

```text
tf
```

with:

```text
tx -> tr
```

5. return `tr`.

Conceptually:

```text
infer(env, f x):
    tf = infer(env, f)
    tx = infer(env, x)
    tr = fresh()
    unify(tf, tx -> tr)
    return tr
```

---

## 20.4 Let

For:

```text
let x = value in body
```

1. infer `value` under the original environment;
2. generalize its type against the original environment;
3. create child environment;
4. bind the generalized scheme to `x`;
5. infer `body` under the child environment.

Conceptually:

```text
infer(env, let x = value in body):
    tv = infer(env, value)
    s = generalize(env, tv)
    env2 = child(env)
    bind(env2, x, s)
    return infer(env2, body)
```

This step supplies let-polymorphism.

---

# 21. Let-recursion

The core should support monomorphic recursive inference.

For:

```text
let rec f = value in body
```

perform:

1. create fresh variable `a`;
2. bind `f : a` monomorphically in a child environment;
3. infer `value` in that environment as `t`;
4. unify `a` with `t`;
5. generalize the resolved recursive type against the outer environment;
6. replace the local monomorphic binding with the generalized scheme;
7. infer `body`.

Conceptually:

```text
a = fresh()

env2 = child(env)
bind(env2, f, mono(a))

t = infer(env2, value)
unify(a, t)

s = generalize(env, prune(a))

env3 = child(env)
bind(env3, f, s)

return infer(env3, body)
```

This supports ordinary HM recursive definitions.

Polymorphic recursion shall require explicit extension outside this specification.

---

# 22. Host-independent inference adapter

The library may optionally expose a callback-based expression interface.

```c
typedef enum hm_expr_kind {
    HM_EXPR_VAR,
    HM_EXPR_LAMBDA,
    HM_EXPR_APPLY,
    HM_EXPR_LET,
    HM_EXPR_LETREC,
    HM_EXPR_CUSTOM
} hm_expr_kind;
```

A host adapter may supply callbacks:

```c
typedef struct hm_expr_ops {
    hm_expr_kind (*kind)(void *expr);

    const char *(*var_name)(void *expr);

    const char *(*lambda_param)(void *expr);
    void *(*lambda_body)(void *expr);

    void *(*apply_function)(void *expr);
    void *(*apply_argument)(void *expr);

    const char *(*let_name)(void *expr);
    void *(*let_value)(void *expr);
    void *(*let_body)(void *expr);

    hm_status (*infer_custom)(
        hm_ctx *ctx,
        hm_env *env,
        void *expr,
        hm_type **result,
        hm_error **error,
        void *userdata
    );

    void *userdata;
} hm_expr_ops;
```

Generic entry point:

```c
hm_status hm_infer(
    hm_ctx *ctx,
    hm_env *env,
    const hm_expr_ops *ops,
    void *expr,
    hm_type **result,
    hm_error **error
);
```

Hosts not requiring this adapter may use the primitive API directly.

---

# 23. Custom expressions

`HM_EXPR_CUSTOM` permits host-language primitives such as:

```text
integer literals
records
lists
conditionals
pattern matching
JSON literals
effects
foreign functions
```

The callback receives direct access to the same HM context and environment.

Example integer literal rule:

```c
*result = hm_type_const(ctx, "Int");
return HM_OK;
```

Example conditional rule:

```text
infer(cond)   ~ Bool
infer(then)   ~ a
infer(else)   ~ a
result = a
```

No modification to the HM core should follow from adding new syntax.

---

# 24. Built-in types

The library shall not hard-code built-in types.

A host may create:

```c
hm_type_const(ctx, "Bool");
hm_type_const(ctx, "Number");
hm_type_const(ctx, "String");
hm_type_const(ctx, "Unit");
hm_type_const(ctx, "Json");
```

Parameterized constructors:

```text
List(a)
Option(a)
Result(a, b)
Map(k, v)
```

remain ordinary constructor applications.

---

# 25. Primitive environments

A host can construct prelude bindings explicitly.

Example:

```text
true  : Bool
false : Bool

id :
    forall a. a -> a

const :
    forall a b. a -> b -> a

map :
    forall a b.
        (a -> b) -> List(a) -> List(b)
```

The API should therefore additionally permit explicit schemes:

```c
hm_scheme *hm_scheme_new(
    hm_ctx *ctx,
    size_t quantified_count,
    hm_type **quantified,
    hm_type *body
);
```

Every quantified argument must reference an unbound variable.

The library shall reject duplicate quantified variables.

---

# 26. Constructor declarations

The core does not require a constructor registry.

For stronger checking, the library may optionally support:

```c
hm_status hm_declare_constructor(
    hm_ctx *ctx,
    const char *name,
    size_t arity
);
```

Then:

```text
List/1
Pair/2
Int/0
->/2
```

can receive fixed arities.

Without registration, equality of constructor name plus arity suffices.

A minimal implementation should omit the registry.

---

# 27. Error representation

Public error kind:

```c
typedef enum hm_error_kind {
    HM_ERR_NONE = 0,
    HM_ERR_NOMEM,
    HM_ERR_MISMATCH,
    HM_ERR_OCCURS,
    HM_ERR_UNBOUND,
    HM_ERR_INVALID,
    HM_ERR_INTERNAL
} hm_error_kind;
```

Inspection:

```c
hm_error_kind hm_error_kind_of(const hm_error *error);
const char *hm_error_message(const hm_error *error);
hm_type *hm_error_left(const hm_error *error);
hm_type *hm_error_right(const hm_error *error);
const char *hm_error_name(const hm_error *error);
```

For mismatch errors:

```text
left
right
```

should identify the incompatible types.

For occurs-check errors:

```text
left  = variable
right = containing type
```

For unbound-name errors:

```text
name
```

shall identify the missing binding.

Errors follow context lifetime.

---

# 28. Error provenance

The generic HM core shall not require source locations.

A host may associate an opaque pointer with the current inference operation:

```c
void hm_ctx_set_origin(
    hm_ctx *ctx,
    void *origin
);

void *hm_error_origin(
    const hm_error *error
);
```

Before inferring a syntax node, the host may set:

```text
origin = AST node / source range / token
```

Errors generated during that operation capture the pointer.

The library shall never dereference it.

---

# 29. Type equality

Two type pointers need not match even when their types match structurally.

Provide:

```c
int hm_type_equal(
    hm_type *a,
    hm_type *b
);
```

The function shall compare pruned structural forms without modifying unification state except path compression.

Examples:

```text
Int == Int

List(Int) == List(Int)

a == a

a != b
```

for two distinct unbound variables.

---

# 30. Type rendering

The library should provide deterministic debugging output.

Callback:

```c
typedef int (*hm_write_fn)(
    void *userdata,
    const char *data,
    size_t size
);
```

Renderer:

```c
hm_status hm_type_write(
    hm_type *type,
    hm_write_fn write_fn,
    void *userdata
);
```

Scheme renderer:

```c
hm_status hm_scheme_write(
    const hm_scheme *scheme,
    hm_write_fn write_fn,
    void *userdata
);
```

Canonical representation:

```text
Int
'a
List('a)
->('a, Bool)
forall 'a 'b. ->('a, 'b)
```

For deterministic variable naming, rendering shall assign display names according to first encounter:

```text
'a
'b
'c
...
'a1
'b1
...
```

Internal variable IDs shall not determine semantic output.

---

# 31. Optional pretty printer

A separate optional layer may render `->/2` specially:

```text
'a -> 'b
```

and selected constructors in familiar syntax:

```text
List 'a
('a * 'b)
```

Such formatting belongs outside the inference core.

---

# 32. Internal representation

A straightforward implementation may use:

```c
struct hm_type {
    int kind;

    union {
        struct {
            unsigned long id;
            struct hm_type *link;
        } var;

        struct {
            char *name;
            size_t arity;
            struct hm_type **args;
        } con;
    } u;
};
```

`link == NULL` denotes an unbound variable.

Constructor nodes remain immutable after construction.

---

# 33. Scheme representation

Suggested representation:

```c
struct hm_scheme {
    size_t count;
    hm_type **vars;
    hm_type *body;
};
```

`vars` lists universally quantified variables.

A scheme shall never mutate.

---

# 34. Environment representation

Suggested representation:

```c
struct hm_binding {
    char *name;
    hm_scheme *scheme;
    struct hm_binding *next;
};

struct hm_env {
    struct hm_env *parent;
    struct hm_binding *bindings;
};
```

A simple linked list suffices for small languages.

Hosts requiring large environments may replace this structure with a hash table without changing the public contract.

---

# 35. Context representation

A context may contain:

```c
struct hm_ctx {
    hm_allocator allocator;

    unsigned long next_var_id;

    void *allocations;

    hm_error *last_error;

    void *origin;
};
```

The actual implementation may use arenas or allocation lists.

All allocations shall get reclaimed by `hm_ctx_destroy()`.

---

# 36. Context lifetime and mutation

A context contains mutable inference state.

The same context shall not support concurrent inference from multiple threads without external synchronization.

Independent contexts may execute concurrently.

Types shall never cross contexts.

Passing a type allocated by context A into an operation using context B constitutes invalid API use.

---

# 37. Transactional inference

Destructive unification creates a practical issue:

```text
attempt inference
encounter error
continue using previous graph
```

Failed unification may leave earlier successful bindings installed.

The minimal API therefore defines failure as invalidating the current speculative inference branch.

A caller shall either:

* discard the context after a failed inference; or
* use an optional rollback facility.

---

# 38. Optional rollback facility

A fuller implementation should support checkpoints.

```c
typedef unsigned long hm_mark;

hm_mark hm_ctx_mark(hm_ctx *ctx);

void hm_ctx_rollback(
    hm_ctx *ctx,
    hm_mark mark
);

void hm_ctx_commit(
    hm_ctx *ctx,
    hm_mark mark
);
```

Every mutation to a variable link shall append an undo record:

```text
variable
old link
```

Rollback restores mutations after the mark.

This enables:

* speculative typing;
* overload resolution outside HM;
* useful diagnostics;
* parser/inference experimentation;
* recovery after type errors.

Rollback does not reclaim arena allocations unless the allocator supports region marks.

The HM semantics do not depend on rollback.

---

# 39. Unification pseudocode

```text
unify(a, b):

    a = prune(a)
    b = prune(b)

    if a == b:
        return success

    if a is variable:
        if occurs(a, b):
            return occurs-error
        link a -> b
        return success

    if b is variable:
        if occurs(b, a):
            return occurs-error
        link b -> a
        return success

    if constructor-name(a) != constructor-name(b):
        return mismatch

    if arity(a) != arity(b):
        return mismatch

    for i = 0 .. arity(a)-1:
        unify(arg(a,i), arg(b,i))

    return success
```

---

# 40. Occurs-check pseudocode

```text
occurs(v, t):

    t = prune(t)

    if t == v:
        return true

    if t is variable:
        return false

    for every argument a of t:
        if occurs(v, a):
            return true

    return false
```

A production implementation should protect against repeated visits when shared type graphs form DAGs.

---

# 41. Instantiation pseudocode

Given:

```text
forall a b . T
```

build a temporary mapping:

```text
a -> fresh()
b -> fresh()
```

Then clone `T` structurally:

```text
instantiate_type(t):

    t = prune(t)

    if t is quantified variable:
        return mapping[t]

    if t is other variable:
        return t

    return constructor(
        name(t),
        instantiate_type(arg1),
        ...
    )
```

Constructor copies created during instantiation may share immutable subtrees when safe.

Correctness does not depend on sharing.

---

# 42. Generalization pseudocode

```text
generalize(env, t):

    A = free_variables(t)
    B = free_variables(env)

    Q = A - B

    return scheme(Q, t)
```

The implementation shall treat variables by identity, not by numeric display name.

---

# 43. Value restriction

Pure Hindley–Milner requires no value restriction.

A host language adding mutable references or effectful expressions may require one.

The HM core shall therefore not generalize automatically.

The host explicitly calls:

```c
hm_generalize(...)
```

at locations allowed by its language semantics.

This separation lets an effectful host implement:

```text
generalize only syntactic values
```

without changing the library.

---

# 44. Extension point: rigid variables

A useful optional feature adds rigid/skolem variables.

```text
flexible variable:
    may unify

rigid variable:
    may only unify with itself
```

API:

```c
hm_type *hm_type_rigid(
    hm_ctx *ctx,
    const char *name
);
```

Rigid variables help implement:

* checking explicit type annotations;
* skolemization;
* signature conformance.

They do not belong to the minimal HM core.

---

# 45. Extension point: annotations

Given source annotation:

```text
expr : forall a. ...
```

a host can:

1. instantiate/skolemize the declared scheme;
2. infer the expression;
3. unify/check the result;
4. reject types less general than required.

This machinery should layer above the core rather than alter unification.

---

# 46. Extension point: records

Plain HM can encode nominal record types as constructors:

```text
Person
Point
Result(a)
```

Structural extensible records require row polymorphism and fall outside this core.

Fixed anonymous record shapes could use canonical constructors:

```text
Record{x:Int,y:Int}
```

but that requires host-defined constructor identity and canonical field ordering.

---

# 47. Extension point: effects

An effect system can reuse the same constructor machinery.

For example:

```text
Fn(arg, effects, result)
Effects(IO, Net)
```

or:

```text
->(arg, Eff(effect_row, result))
```

However effect-row polymorphism exceeds ordinary HM when effects support open rows.

The generic core should expose enough low-level construction and unification primitives for a separate effect layer.

---

# 48. Extension point: constraints

Type classes or overloaded operators require qualified types:

```text
forall a. Num(a) => a -> a -> a
```

This specification excludes constraint solving.

A future extension may define:

```c
hm_constraint
hm_qualified_scheme
```

without changing the fundamental type/unification API.

---

# 49. Complexity

For ordinary programs:

* fresh-variable creation: O(1);
* constructor creation: O(arity);
* pruning with path compression: near-constant amortized;
* unification: approximately proportional to traversed type structure;
* occurs checking: proportional to examined type graph;
* generalization: proportional to the target type plus environment free-variable set;
* instantiation: proportional to the scheme body traversed.

Naive environment free-variable recomputation may dominate inference for large environments.

An optimized implementation may cache free-variable sets per scheme and environment scope.

---

# 50. Required invariants

The implementation shall preserve the following invariants.

## 50.1 Variable link invariant

A variable link points to either:

* `NULL`; or
* a valid type in the same context.

---

## 50.2 Acyclic type graph invariant

Variable linking plus occurs checking shall never create a cycle.

---

## 50.3 Constructor immutability

After creation:

```text
name
arity
arguments
```

of a constructor shall not change.

---

## 50.4 Scheme immutability

A scheme's quantified-variable set and body shall not change.

The body may contain flexible variables whose links later change only when those variables remain free rather than quantified.

---

## 50.5 Context identity

Every reachable HM object belongs to exactly one context.

---

# 51. Minimal public API

A compact first implementation can expose only:

```c
typedef struct hm_ctx hm_ctx;
typedef struct hm_type hm_type;
typedef struct hm_scheme hm_scheme;
typedef struct hm_env hm_env;
typedef struct hm_error hm_error;

hm_ctx *hm_ctx_new(const hm_allocator *allocator);
void hm_ctx_destroy(hm_ctx *ctx);

hm_type *hm_type_var(hm_ctx *ctx);
hm_type *hm_type_const(hm_ctx *ctx, const char *name);

hm_type *hm_type_con(
    hm_ctx *ctx,
    const char *name,
    size_t arity,
    hm_type **args
);

hm_type *hm_type_fun(
    hm_ctx *ctx,
    hm_type *arg,
    hm_type *result
);

hm_type *hm_type_prune(hm_type *type);

hm_status hm_unify(
    hm_ctx *ctx,
    hm_type *a,
    hm_type *b,
    hm_error **error
);

hm_scheme *hm_scheme_mono(
    hm_ctx *ctx,
    hm_type *type
);

hm_scheme *hm_scheme_new(
    hm_ctx *ctx,
    size_t count,
    hm_type **vars,
    hm_type *body
);

hm_type *hm_instantiate(
    hm_ctx *ctx,
    const hm_scheme *scheme
);

hm_scheme *hm_generalize(
    hm_ctx *ctx,
    hm_env *env,
    hm_type *type
);

hm_env *hm_env_new(hm_ctx *ctx);

hm_env *hm_env_child(
    hm_ctx *ctx,
    hm_env *parent
);

hm_status hm_env_bind(
    hm_env *env,
    const char *name,
    hm_scheme *scheme
);

hm_scheme *hm_env_lookup(
    hm_env *env,
    const char *name
);
```

This API suffices to implement Algorithm W in the host.

---

# 52. Example: polymorphic identity

Construct:

```text
fun x -> x
```

Inference:

```text
a = fresh()

x : a

body x
    => a

lambda
    => a -> a
```

At a let binding:

```text
let id = fun x -> x
```

generalization yields:

```text
forall a. a -> a
```

Separate uses instantiate independently:

```text
id 1
    => Int

id true
    => Bool
```

---

# 53. Example: application

Expression:

```text
fun f -> fun x -> f x
```

Steps:

```text
f : a
x : b

f x:
    fresh c
    unify a with b -> c

therefore:

f : b -> c

whole expression:

(b -> c) -> b -> c
```

Generalized:

```text
forall b c.
    (b -> c) -> b -> c
```

---

# 54. Example: occurs failure

Expression:

```text
fun x -> x x
```

Infer:

```text
x : a
```

Application requires:

```text
a ~ a -> b
```

Occurs check finds `a` inside:

```text
a -> b
```

Inference fails with:

```text
HM_ERROR_OCCURS
```

This rejects the untyped self-application required by the omega combinator.

---

# 55. Example: primitive list functions

Host prelude:

```text
nil :
    forall a.
    List(a)

cons :
    forall a.
    a -> List(a) -> List(a)

head :
    forall a.
    List(a) -> a
```

No special list support occurs inside the HM engine.

---

# 56. Testing requirements

A conforming implementation shall test at least the following.

## 56.1 Freshness

Two calls to:

```c
hm_type_var(ctx)
```

produce distinct variables.

---

## 56.2 Reflexive unification

```text
a ~ a
```

succeeds.

---

## 56.3 Variable binding

```text
a ~ Int
```

succeeds and pruning `a` returns `Int`.

---

## 56.4 Constructor equality

```text
List(Int) ~ List(Int)
```

succeeds.

---

## 56.5 Constructor mismatch

```text
Int ~ Bool
```

fails.

---

## 56.6 Arity mismatch

```text
T(Int) ~ T(Int, Bool)
```

fails.

---

## 56.7 Recursive unification

```text
Pair(a, Int) ~ Pair(Bool, b)
```

produces:

```text
a = Bool
b = Int
```

---

## 56.8 Occurs check

```text
a ~ List(a)
```

fails.

---

## 56.9 Scheme instantiation freshness

Instantiating:

```text
forall a. a -> a
```

twice produces unrelated variables.

---

## 56.10 Monomorphic scheme sharing

Instantiating a monomorphic scheme:

```text
a -> a
```

shall preserve `a` rather than create a fresh variable.

---

## 56.11 Generalization

With empty environment:

```text
a -> a
```

generalizes to one quantified variable.

---

## 56.12 Environment-sensitive generalization

Given:

```text
x : a
```

the type:

```text
a -> b
```

generalizes only `b`.

---

## 56.13 Identity

Inference for:

```text
fun x -> x
```

produces:

```text
a -> a
```

---

## 56.14 Constant function

```text
fun x -> fun y -> x
```

produces:

```text
a -> b -> a
```

---

## 56.15 Apply

```text
fun f -> fun x -> f x
```

produces:

```text
(a -> b) -> a -> b
```

modulo variable renaming.

---

## 56.16 Let polymorphism

```text
let id = fun x -> x in
Pair(id 1, id true)
```

produces:

```text
Pair(Int, Bool)
```

---

## 56.17 Lambda monomorphism

```text
fun id ->
    Pair(id 1, id true)
```

shall fail unless `id` receives a suitable explicitly polymorphic extension.

Lambda-bound variables remain monomorphic in HM.

---

## 56.18 Self application

```text
fun x -> x x
```

fails via occurs check.

---

# 57. Conformance boundary

An implementation conforms to this specification when it provides:

1. context-scoped memory management;
2. fresh flexible type variables;
3. named n-ary type constructors;
4. destructive or semantically equivalent unification;
5. occurs checking;
6. monomorphic and quantified schemes;
7. instantiation;
8. generalization relative to an environment;
9. lexical environments;
10. rank-1 Hindley–Milner behavior equivalent to Algorithm W.

Exact internal data structures need not match this document.

---

# 58. Recommended source layout

```text
include/
    hm.h

src/
    hm_ctx.c
    hm_type.c
    hm_unify.c
    hm_scheme.c
    hm_env.c
    hm_ftv.c
    hm_print.c
    hm_error.c
    hm_internal.h

tests/
    test_type.c
    test_unify.c
    test_scheme.c
    test_env.c
    test_infer.c
```

A very small implementation may combine all source files into:

```text
hm.c
hm.h
```

without changing the API.

---

# 59. Recommended first implementation

For the initial implementation:

* use an allocation list owned by `hm_ctx`;
* use linked-list environments;
* use mutable variable links;
* perform full occurs checks;
* represent free-variable sets as dynamic arrays of `hm_type *`;
* use linear lookup for those sets;
* omit rollback;
* omit constructor interning;
* omit rigid variables;
* omit callback-driven AST inference;
* implement only the primitive generic API.

This keeps the semantic core small.

A practical C89 implementation should fit in approximately:

```text
hm.h            150–250 LOC
hm.c            700–1200 LOC
tests           500–1000 LOC
```

without compromising the Hindley–Milner model.

---

# 60. Core conceptual boundary

The library should implement exactly this layer:

```text
                 host language
                      |
          +-----------+-----------+
          | literals / AST / let  |
          | effects / records ... |
          +-----------+-----------+
                      |
                      v
             +----------------+
             | generic HM API |
             +----------------+
             | environments   |
             | schemes        |
             | generalization |
             | instantiation  |
             | unification    |
             | occurs check   |
             | type graph     |
             +----------------+
                      |
                      v
                  C89 runtime
```

This boundary keeps the library genuinely generic.

The host defines syntax and language semantics; the HM library supplies rank-1 polymorphic inference and type equality constraints.

I would make the **primitive API in §51 the actual v1 contract** and leave generic AST inference outside the library. That keeps HM reusable for your JSON+HM+DAG+ML language, Hoodlum, or unrelated experiments without coupling the library to an expression model.

