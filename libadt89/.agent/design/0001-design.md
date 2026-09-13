# Design

## Module boundaries

- `adt_ctx.c` — context lifecycle, allocator, registries, failure helper.
- `adt_error.c` — structured error accessors.
- `adt_type.c` — declarations, nominal identity, parameters, application.
- `adt_ctor.c` — constructor declarations.
- `adt_scheme.c` — sealing validation + constructor-scheme generation.
- `adt_bind.c` — installing constructors into an `hm_env`.
- `adt_pattern.c` — pattern objects + constructor-pattern typing.
- `adt_match.c` — exhaustiveness and redundancy (Maranget usefulness).

## Ownership model

- `adt_ctx` owns declarations, constructors, patterns, and its last error,
  each individually freed by `adt_ctx_destroy` via the ADT allocator.
- HM objects (`hm_type`, `hm_scheme`, `hm_env`) live in the HM arena and are
  never freed by ADT code. Destroy order: ADT first, then HM.
- Errors record a status + a `const hm_error *`; messages are static literals.

## Nominal identity

`adt_type_new` draws a process-wide unique id and stores a private canonical
HM name (`adt_<id>`). `adt_type_apply` builds `hm_type_con(hm, hm_name, ...)`.
Because HM compares constructors by name+arity, distinct declarations never
unify while applications of one declaration always do. Display names are not
required unique.

## Scheme generation

For parameters `α1..αn` and constructor `C(F1..Fm)`, sealing builds the
result `T(α1..αn)`, then a right-associated function body
`F1 -> ... -> Fm -> T`, then `hm_scheme_new(hm, n, αs, body)`. All datatype
parameters are quantified, including those absent from a constructor's fields.
Recursive references are nominal applications; declaration never unifies, so
the HM occurs check is never triggered by declaration.

## Pattern typing

`adt_pattern_types` instantiates the constructor scheme, decomposes its
curried function type via the `->` (arity 2) constructor into fields and
result, unifies the result against the scrutinee, and returns the pruned
field types. Pure HM unification; no datatype-specific logic.

## Match analysis

Patterns are wildcards or constructor applications. Exhaustiveness asks
whether the wildcard vector stays useful after all clauses; a clause is
redundant when it is not useful after the preceding clauses. The algorithm
expands constructors by pattern depth, never enumerating values, so recursive
types terminate.
