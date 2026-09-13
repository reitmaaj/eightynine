# libcat89 acceptance tests

## 0001-acceptance.md

The library MUST exhibit the behavior listed under "must exhibit"; it MUST
reject, avoid, refuse, or fail safely on the behavior listed under "must
reject".

## Must exhibit

- A minimal category core exposing exactly dom/cod/identity/compose plus
  structural object identity and morphism lifetime, with no derived structure
  in `core.h`.
- Generic composition with the uniform convention `out = g o f`.
- Generic pre-validation of composition typing returning `CAT89_DOMAIN` on
  endpoint mismatch without invoking the backend compose callback.
- One owned morphism reference returned through every `cat89_mor **out`; one
  borrowed handle returned through every `const cat89_mor *` / `const
  cat89_obj *` return.
- Stable object handles valid for the category lifetime (all v1 backends).
- Correct category/morphism/capability reference counting with destruction of
  the backend exactly once after the last dependent releases it.
- Object/morphism equality and enumeration only through explicit optional
  capability objects; core operation never requires them.
- Derived structures (iso, split mono/epi, functor, nat, cone, limit) that wrap
  or reference ordinary morphisms and never decorate them.
- Every generic wrapper category (opposite, product, arrow, slice, coslice,
  comma) behaving as an ordinary `cat89_category` implementable with only the
  core interface.
- Wrapper categories (product, comma, slice/coslice, arrow) exposing borrowed
  component accessors so composite object/morphism equality and enumeration can
  be supplied externally without adding equality or enumeration to the wrapper.
- Finite, free, and thin reference backends whose object handles remain valid
  for the category lifetime and whose morphisms honour retain/release.
- Finite exhaustive category-law checking that passes all reference fixtures
  and matches an independently written exhaustive loop.
- Universal constructions that represent their factorization property and pass
  exhaustive existence-and-uniqueness checks over a finite universe.
- A strict ISO C89 build, clean under the sibling `green` matrix, with all
  public headers compiling standalone.
- Finite universal-construction finders (W4.2) that, over an enumerated finite
  ambient with an explicit `cat89_eq`, return a genuine
  `cat89_limit`/`cat89_colimit` carrying a real universal `factor` for binary
  products/coproducts, equalizers/coequalizers, and pullbacks/pushouts.
- Exhaustive library law-family sweeps (W5) that automatically enumerate and
  verify the iso, split mono/epi, functor identity/composition, naturality, and
  cone/cocone law cases over a finite ambient, in addition to the exhaustive
  identity/associativity category-law checker.

## Must reject

- Composition of morphisms whose codomain/domain mismatch under the backend's
  structural object identity -> `CAT89_DOMAIN`.
- Constructing a category with any required callback (dom/cod/identity/compose/
  obj_same/mor_retain/mor_release) omitted -> `CAT89_INVALID`, `*out == NULL`.
- NULL category/morphism/object/required callback/out-parameter passed to a
  fallible API -> `CAT89_INVALID` (release functions no-op on NULL).
- Re-tagging an allocator failure from a callback as `CAT89_CALLBACK`; statuses
  MUST propagate verbatim (`CAT89_NOMEM` stays `CAT89_NOMEM`).
- Silent success of any checker when a capability the checker requires
  (equality, shape enumeration) is unavailable; it MUST fail explicitly with
  `CAT89_NOT_SUPPORTED`.
- A category claiming a limit/colimit whose mediating morphism is not unique
  over the finite test universe, and any universal construction reduced to a
  bare object+legs with no factorization.
- Deliberately malformed fixtures (violating left/right identity, associativity,
  functor composition/identity, naturality, iso inverse laws, split laws, cone
  commutativity) passing the corresponding checker.
- Mixing morphisms/objects/categories across distinct category instances where
  the API requires instance identity -> rejected (extensionally equal but
  distinct instances are NOT the same category).
- Collapsing componentwise-equal product/comma morphisms that are distinct
  constructions onto a single handle: they must stay distinct refcounted
  handles that compare equal only under externally supplied equality (no
  implicit collapse).
- Implicit collapse of distinct-but-isomorphic objects or of distinct paths in
  the free category that merely share endpoints.
- Any first-class derived structure modifying the representation or dispatch of
  an ordinary morphism, and any runtime `kind` tag or morphism flag for
  derived properties.
- Leaking memory or leaving live allocations after an injected allocation
  failure; partial wrapper objects escaping on failure; caller-owned reference
  counts changing on failure.
- A universal-construction finder silently succeeding when the `cat89_eq`
  capability it requires is absent, or returning a bogus "limit" object+legs
  with no usable factorization: on absence it MUST return `CAT89_NOT_FOUND`
  with `*out == NULL`; on an unusable argument it MUST return `CAT89_INVALID`
  with `*out == NULL`; a returned limit/colimit MUST carry a working factor.
- A finite finder claiming an object X is the product apex when a genuine
  product apex P exists and X lacks the universal mediating arrow (no
  wrongly-substituted apex).
- Any exhaustive law-family sweep reporting a silent partial pass when a
  required capability (equality, or a shape/enumeration over the ambient or
  shape category) is unavailable: it MUST fail with `CAT89_NOT_SUPPORTED`.

## Must exhibit (W6 hardening continuation)

- The generic wrapper-category and derived-structure constructors (opposite,
  product, functor new/identity, nat new/identity, iso new/identity, split
  mono/epi new/identity, cone/cocone, limit/colimit, arrow, slice, coslice,
  comma) are error-atomic under a fault-injecting allocator: every swept
  single-allocation failure returns `CAT89_NOMEM` with `*out == NULL`, no
  partial object escapes, and the run is leak-free under ASan+LSan.
- Every mock backend callback failure mode (`fail_dom/cod/compose/identity/
  retain` returning `CAT89_NOMEM` or `CAT89_CALLBACK`) propagates verbatim to
  the caller through wrapper composition and core dom/cod/identity, with no
  live allocation left after the failing call.

## Must reject (W6 hardening continuation)

- A wrapper or derived constructor leaving a live allocation or a partial (non-
  NULL but unusable) handle when an allocation it makes fails.
- Any layer retagging a backend callback failure: a callback returning
  `CAT89_NOMEM` surfaced through wrapper composition MUST NOT be observed as
  `CAT89_CALLBACK` (statuses propagate verbatim).

## Must exhibit (W7 ABI/architecture audit)

- `core.h` includes no derived header; a TU including only `<cat89/core.h>`
  compiles clean in strict C89.
- The public-header `<cat89/...>` include graph is acyclic.
- Every defined global symbol in the shipped static library carries the
  `cat89_` prefix and is declared in a public header (or `cat89_internal.h`);
  no test/fixture symbol leaks into the library archive.

## Must reject (W7 ABI/architecture audit)

- A public-header include cycle (a layering violation).
- A non-`cat89_` or header-undeclared global symbol exported by the library, and
  any test/fixture symbol bundled into the shipped `libcat89.a`.

## Must exhibit (bugfix pass)

- Every fallible derived-structure binary op (`cat89_iso_compose`,
  `cat89_split_mono_compose`, `cat89_split_epi_compose`, `cat89_iso_invert`,
  `cat89_iso_as_split_mono`, `cat89_iso_as_split_epi`) sets `*out` to NULL
  before validating its arguments.

## Must reject (bugfix pass)

- A fallible derived-structure binary op returning `CAT89_INVALID` while leaving
  a stale non-NULL `*out` that the caller could mistake for an owned handle:
  on any NULL-handle or cross-category-instance failure it MUST leave
  `*out == NULL`.
