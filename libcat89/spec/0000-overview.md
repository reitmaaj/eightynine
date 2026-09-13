# libcat89 normative summary

This directory records the normative architecture of `libcat89`. The detailed,
authoritative design documents (specification, Interface Control Document, test
plan, implementation plan) are condensed into the working contract under
`.agent/`; the most important rules are restated here so a future session can
reconstruct the full intent.

## Layering (normative, acyclic)

    status / alloc -> core
    core -> {eq, enum}                capabilities (orthogonal)
    core -> functor -> nat
    core -> iso -> split
    core -> opposite, product
    functor -> comma -> slice/coslice/arrow
    functor -> diagram -> cone -> limit
    check depends on core + eq + enum
    finite/free/thin implement core (+ eq/enum where offered)

`core.h` must never include a higher-level header. No derived structure may
impose an ABI requirement on a lower layer.

## Invariants (abridged)

- CAT-I1 minimal category kernel (dom/cod/identity/compose plus raw-handle
  provenance probes).
- CAT-I2 core depends on no derived structure.
- CAT-I3 derived structures wrap ordinary morphisms; no morphism subclass/flag.
- CAT-I4 capabilities (equality/enumeration/hash/finite) are optional.
- CAT-I5 no decidable-equality assumption in generic operations.
- CAT-I6 no finiteness/enumeration assumption in the kernel.
- CAT-I7 morphisms need not execute.
- CAT-I8 laws are explicit contracts; checking is separate.
- CAT-I9 universal properties are first-class (factorization, not bare cones).
- CAT-I10 no implicit equivalence collapse (isomorphic stays distinct).
- CAT-I11 raw handles are valid only in the category that owns them; generic
  algorithms use categorical object identity, never C pointer identity.
- CAT-I11 mathematical definitions guide layering (reuse, no parallel machinery).
- CAT-I12 application semantics stay outside the generic core.

## Execution order (freeze gates)

core -> finite oracle + exhaustive checker (freeze F1: freeze status/alloc/core)
-> functor/nat/iso/split -> free/thin -> opposite/product/comma/slice/arrow
-> diagram/cone/limit + universal constructions -> hardening -> ABI audit.

The finite backend + exhaustive checker must land early because they are the
reusable oracle for every higher layer.

## Completion status (W1-W8)

All workstreams in scope are landed on `main`:

- W1-W4: kernel, algebra, wrapper categories, shapes, and the finite
  universal-construction finders (binary product/coproduct, equalizer/
  coequalizer, pullback/pushout) returning genuine `cat89_limit`/`cat89_colimit`
  over the relevant shape diagram with a working universal `factor`.
- W5: the exhaustive category-law checker and the exhaustive derived
  law-family sweeps (iso, split mono/epi, functor, naturality, cone/cocone),
  each `CAT89_NOT_SUPPORTED` when a required capability is absent.
- W6: hardening — mock observer/ctx split, allocator-failure atomicity across
  every constructor and the finders, a callback-failure/status-propagation
  matrix, and a clean ASan+UBSan+LSan suite (`just sanitize`).
- W7: ABI/architecture audit — `core.h` isolation, header-DAG acyclicity,
  exported-symbol inventory, struct-exposure review (see `spec`, enforced by
  `just arch`, which also wired into `just lint`).
- W8: documentation closure (README, spec, `.agent` scenarios/acceptance).

Deferred to v2 (not in scope): adjunctions, monads, monoidal/enriched settings,
profunctors/presheaves, Kan extensions, fibrations, higher categories, toposes,
model categories, and proof terms.
