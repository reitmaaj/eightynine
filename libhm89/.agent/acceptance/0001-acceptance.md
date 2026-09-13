# Acceptance criteria

Acceptance tests encode behavior the software MUST exhibit and behavior that
would be UNACCEPTABLE (must be rejected / fail safely / be reported as
errors). Each requirement traces to `.agent/testing/0001-scenarios.md` and to
`spec/hm-spec.md`.

## Must exhibit (exhibit)

- Given a context, two `hm_type_var` calls MUST yield distinct nodes with
  distinct ids. (§56.1)
- Unifying the same variable with itself MUST return `HM_OK`. (§56.2)
- Unifying a variable `a` with `Int` MUST succeed and pruning `a` MUST yield
  `Int`. (§56.3)
- Unifying `List(Int)` with `List(Int)` MUST succeed. (§56.4)
- Unifying `Pair(a, Int)` with `Pair(Bool, b)` MUST succeed and bind
  `a = Bool`, `b = Int`. (§56.7)
- A monomorphic scheme `a -> a` MUST share `a` across instantiations. (§56.10)
- Instantiating `forall a. a -> a` twice MUST produce unrelated fresh
  variables. (§56.9)
- Generalizing `a -> a` under an empty environment MUST yield one quantified
  variable. (§56.11)
- Generalizing `a -> b` under an environment binding `x : a` MUST quantify
  only `b`. (§56.12)
- Inferring `fun x -> x` MUST yield `a -> a`; `fun x -> fun y -> x` MUST yield
  `a -> b -> a`; `fun f -> fun x -> f x` MUST yield `(a -> b) -> a -> b`
  modulo renaming. (§56.13–56.15)
- Inferring `let id = fun x -> x in Pair(id 1, id true)` MUST succeed with
  result `Pair(Int, Bool)`. (§56.16)
- Environment lookup MUST find parent bindings, and a child binding MUST
  shadow an ancestor binding. (§15)
- An environment MUST copy bound names; a constructor MUST copy its name.
- `hm_ctx_destroy` MUST reclaim every allocation made through the context,
  for both the default allocator and a caller-supplied allocator. (§6, §35)
- Type/scheme rendering MUST assign deterministic display names by first
  encounter. (§30)
- Reading a constructor's name, arity, and arguments MUST report the
  underlying `{name, arity, args}` for an n-ary constructor, including after
  path compression (a variable unified onto a constructor prunes to it). The
  three introspection accessors MUST NOT expose internal headers.

## Must reject / fail safely (reject)

- Unifying `Int` with `Bool` MUST fail with `HM_ERROR_MISMATCH`. (§56.5)
- Unifying `T(Int)` with `T(Int, Bool)` MUST fail with `HM_ERROR_MISMATCH`.
  (§56.6)
- Unifying a variable with a type containing it — `a ~ List(a)`, and
  `a ~ ->(Int, a)` — MUST fail with `HM_ERROR_OCCURS`; the type graph MUST
  remain acyclic (never link a variable to itself or to a type containing
  it). (§13, §50.2)
- Inferring `fun x -> x x` MUST fail via the occurs check (§56.18), and
  inferring `fun id -> Pair(id 1, id true)` with a monomorphic `id` MUST fail
  (`HM_ERROR_MISMATCH`), since lambda-bound variables are monomorphic
  (§56.17).
- Constructing a scheme with a duplicate quantified variable, or with a
  quantified argument that is not an unbound variable, MUST be rejected.
  (§25)
- Reading the variable id of a non-variable is invalid API use and MUST be
  reported as such (no crash); the function MAY return 0 but MUST NOT
  interpret constructor fields as a variable. (§8)
- Querying constructor introspection on a non-constructor type MUST NOT crash:
  the name accessor returns `NULL`, the arity accessor returns 0, and the arg
  accessor returns `NULL`. Querying an out-of-range argument index MUST return
  `NULL` rather than reading out of bounds. (Regression guard for the
  public constructor-introspection surface.)
- Passing a type allocated by one context into an operation of another
  context is invalid API use; the library MUST NOT crash on it in ways that
  corrupt memory (the contract forbids it; behavior otherwise is
  unspecified). (§36) [Not exercised destructively in tests.]
- Allocator failures MUST surface as `HM_ERROR_NOMEM` / `HM_ERR_NOMEM`, never
  as a crash from an unchecked `NULL` dereference. Allocation of zero bytes
  MUST NOT rely on implementation-defined `malloc(0)`. (§6, §58)
- Errors MUST NOT be dereferenced as nodes; error inspection uses the
  dedicated accessors only. (§27)

## Conformance boundary (spec §57)

An implementation conforms when it provides: context-scoped memory
management; fresh flexible variables; named n-ary constructors; destructive
or equivalent unification; occurs checking; monomorphic and quantified
schemes; instantiation; generalization relative to an environment; lexical
environments; rank-1 Algorithm-W-equivalent behavior. Each of these is
covered by at least one scenario in `.agent/testing/0001-scenarios.md` and by
the tests under `test/`.
