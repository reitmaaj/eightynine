# libcat89 testing scenarios (BDD) - correctness-closure increment

## 0011-provenance-correctness.md

This file records the scenarios for the correctness/provenance increment:
raw-handle provenance, representation independence, finite-builder atomicity,
checked arithmetic, one-pass finder collection and unambiguous factorization.
No new categorical features are introduced.

## Functor composition

- SCENARIO composition target: GIVEN `F : C -> D` and `G : D -> E` with three
  distinct category instances WHEN `cat89_functor_compose(G, F, ...)` runs THEN
  `source == C`, `target == E` and `GF(x) == G(F(x))`, `GF(f) == G(F(f))`.
- SCENARIO incompatible middle: GIVEN `F : C -> D1`, `G : D2 -> E`, `D1 != D2`
  WHEN composing THEN `CAT89_INVALID` and `*out == NULL` with no retained
  references.

## Raw-handle provenance

- SCENARIO ownership contract: GIVEN a category WHEN `cat89_category_new` is
  called with `owns_obj == NULL` or `owns_mor == NULL` THEN `CAT89_INVALID`.
- SCENARIO null ownership: `cat89_owns_obj(C, NULL) == 0` and
  `cat89_owns_mor(C, NULL) == 0` for every backend.
- SCENARIO foreign handle rejected before callbacks: GIVEN a live handle owned
  by category A WHEN it is passed to category B THEN the operation returns
  `CAT89_INVALID` (0 for `obj_same`, no-op for release) and no representation
  callback of B is entered.
- SCENARIO safe foreign ownership probe: GIVEN any live foreign handle WHEN
  `owns_obj`/`owns_mor` runs THEN it does not dereference, retain, release or
  inspect the candidate and does not crash under ASan/UBSan.
- SCENARIO compose gating order: GIVEN a foreign `f` or `g` WHEN composing THEN
  both ownership checks happen before `cod(f)`/`dom(g)`; the counted backend
  observes only ownership callbacks.
- SCENARIO core results are owned: `dom`, `cod`, `identity`, `compose` results
  satisfy `owns_obj`/`owns_mor` for every built-in backend.

## Public API provenance

- SCENARIO equality provenance: `cat89_obj_equal`/`cat89_mor_equal` reject a
  foreign operand with `CAT89_INVALID`, `*out_equal == 0` and zero equality
  callback invocations.
- SCENARIO hom enumeration provenance: `cat89_hom_iter_open` rejects foreign
  domain/codomain before the iterator-open callback.
- SCENARIO functor/naturality provenance: `cat89_functor_map_obj`,
  `cat89_functor_map_mor` and `cat89_nat_component` reject a foreign source
  handle before mapping/component callbacks.
- SCENARIO cone/cocone provenance: apex must belong to the diagram target;
  leg shape objects must belong to the diagram source.
- SCENARIO iso/split provenance: constructors reject foreign morphisms before
  retaining them.
- SCENARIO derived constructors: product/comma/slice/coslice/opposite reject
  foreign component objects/morphisms before casting or retaining.
- SCENARIO derived accessors: every component accessor takes its owning
  category, returns NULL for NULL or foreign handles, and never casts a foreign
  handle.

## Typed capability provenance

- SCENARIO capability ownership: an equality/enumeration capability bound to a
  different category instance is rejected with `CAT89_INVALID` before any
  equality/iterator callback runs, for slice/coslice constructors, local law
  checks, cone/cocone checks, exhaustive checks, and all finders.
- SCENARIO finite finder arguments: foreign objects/morphisms passed to the
  universal-construction finders are rejected with `CAT89_INVALID` before
  `dom`, `cod`, equality or enumeration.

## Representation independence

- SCENARIO alias objects: GIVEN distinct object pointers that satisfy
  `cat89_obj_same` and morphisms whose endpoints are distinct-but-equal
  representatives WHEN `cat89_compose` runs THEN composition succeeds.
- SCENARIO alias unique arrows: GIVEN a unique arrow whose stored endpoints are
  alternate representatives WHEN a finder searches with the canonical
  representatives THEN it finds the arrow.
- SCENARIO alias exhaustive checks: GIVEN composable morphisms whose middle
  endpoints are distinct-but-equal pointers WHEN the exhaustive checker runs
  THEN no false composition or law failure is reported.

## Finite builder

- SCENARIO failure atomicity: GIVEN any allocation point of
  `cat89_finite_add_object`, `cat89_finite_add_morphism` or
  `cat89_finite_set_composition` WHEN it fails with `CAT89_NOMEM` THEN the
  builder is exactly as usable as before; retrying succeeds and yields the
  expected next id.
- SCENARIO validation: GIVEN a finite table WHEN `cat89_finite_validate` runs
  THEN it returns `CAT89_OK` with `*out_valid == 1` exactly for a complete,
  typed, identity/associativity-respecting finite category and `*out_valid == 0`
  for missing/mistyped identities, missing/non-composable/mistyped composition,
  duplicate keys, or broken identity/associativity.
- SCENARIO build ownership: `cat89_finite_build` never consumes or releases the
  builder on success or failure; an invalid table yields `CAT89_INVALID` with
  all outputs NULL and the builder untouched; the same builder can build twice
  or be mutated after a successful build.
- SCENARIO checked arithmetic: size addition/multiplication overflow, vector
  capacity growth overflow and reference-count overflow all fail without
  mutation or out-of-bounds allocation.

## Finders and factorization

- SCENARIO one-pass object collection: `cat89_find_terminal`/`cat89_find_initial`
  open the object enumeration exactly once; a second opening fails.
- SCENARIO checked census sizes: finder census/vector allocations use checked
  arithmetic and route through the caller allocator.
- SCENARIO factorization domain: `cat89_limit_factor`/`cat89_colimit_factor`
  reject a candidate over a different diagram with `CAT89_INVALID` before the
  factor callback.
- SCENARIO mediator cardinality: finder-created factor callbacks return
  `CAT89_OK` with one owned mediator, and `CAT89_NOT_FOUND` with NULL for zero
  or multiple mediators; `CAT89_OK + NULL` is never returned.

## Failure behavior

- SCENARIO failure empties outputs: every status-returning API with an output
  pointer leaves it NULL/zero on failure, including `CAT89_INVALID`,
  `CAT89_DOMAIN`, `CAT89_NOT_FOUND`, `CAT89_NOT_SUPPORTED` and `CAT89_NOMEM`.
- SCENARIO status distinction: foreign handle -> `CAT89_INVALID`; owned but
  non-composable -> `CAT89_DOMAIN`; absent optional capability ->
  `CAT89_NOT_SUPPORTED`; no unique finite solution -> `CAT89_NOT_FOUND`.

## Deliberate scope notes

- The malformed-law finite fixtures used by checker tests are built through the
  internal `cat89_finite_build_unchecked` hook; the public
  `cat89_finite_build` always validates. This keeps non-categorical finder
  oracles available without a public corruption API.
- A genuine finder-returned limit admits at most one mediator for any candidate
  cone, so a factor callback can observe zero or one mediator but never two.
  FM03/FM07 (two mediators) are therefore exercised at the finder level: a
  non-universal apex is rejected with `CAT89_NOT_FOUND`, while a candidate that
  is not a valid cone yields zero mediators and `CAT89_NOT_FOUND` with NULL.
