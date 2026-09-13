# libhm89 — a green-compliant generic Hindley–Milner inference library

`libhm89` is a strict-ISO-C89, dependency-free implementation of rank-1
Hindley–Milner type inference. It exposes a small primitive API over its own
type graph: fresh type variables, named n-ary type constructors, mutable-link
unification, occurs checking, type schemes with universal quantification,
generalization and instantiation relative to a lexical environment, and
structured error reporting.

## Positioning

The library is a genuinely generic inference core. It owns no AST, source
language, or primitive type names; a host supplies syntax and semantics and
uses the library's primitives to implement Algorithm W (§19–20 of the spec).
The type graph is host-independent: function types are ordinary constructor
applications, conventionally `->(arg, result)`.

## Why strict C89 + green

The project inhabits the strict `C89 ∩ C23` intersection under both GCC and
Clang with explicit semantic structure and one canonical Allman format — the
sibling `green` profile. This discipline is a good fit for a library whose
correctness rests on subtle graph invariants (acyclic type graph, context
identity, variable-link invariant): explicit statement-level structure and
short pure helpers make every store, effect, and control decision inspectable
and unit-testable.

## Scope (V1)

- Context-scoped allocation with an optional custom allocator (§6).
- Fresh flexible type variables with stable ids (§8).
- Named n-ary type constructors; `->` is an ordinary constructor (§9).
- Convenience helpers: const, app1, app2, fun (§10).
- Path-compressing dereference `hm_type_prune` (§11).
- Destructive unification with occurs checking (§12–13).
- Monomorphic and quantified schemes (§14).
- Lexical environments with child scoping and shadowing (§15).
- Generalization `ftv(type) - ftv(env)` (§16–17).
- Instantiation with fresh variables for quantified ones (§18).
- Structured error reporting with kind, message, left/right/name, origin (§27–28).
- Deterministic type/scheme rendering (§30).

## Out of scope (V1)

- Any AST or source-language adapter (`hm_infer`, `hm_expr_ops`).
- Rollback/checkpoints (`hm_ctx_mark`/`rollback`/`commit`).
- Rigid/skolem variables, value restriction enforcement, type classes,
  records, effects, constraints.
- Constructor registry / interning.
- Built-in or host-predefined types.
