# Stakeholders

## A host-language implementor embedding rank-1 inference

**As** a developer building a small statically-typed language (AST + parser)
on top of a generic type checker,
**I want** a minimal primitive API for variables, constructors, unification,
generalization, instantiation, schemes, and environments that does not couple
to my AST,
**so that** I can implement Algorithm W myself and stay free to choose syntax,
built-in types, and semantics.

## The type-inference core maintainer

**As** the maintainer of `libhm89`,
**I want** the implementation to compile warning-clean as strict C89 and
strict C23 under both GCC and Clang with explicit semantic structure,
**so that** the library is portable, long-lived, inspectable, and verifiable
by the `green` toolchain.

## A student or tool author learning HM

**As** someone studying Hindley–Milner,
**I want** a small, readable reference that renders deterministic debugging
output and rejects ill-typed programs (occurs, mismatch, unbound),
**so that** I can inspect inference step by step and trust the model.

## A host combining HM with records/effects/constraints later

**As** a host that will later add qualified types or effect rows,
**I want** the V1 core to leave generalization as an explicit, host-called
operation and to expose low-level construction/unification primitives,
**so that** value restriction and extension layers build on top without
modifying the core.
