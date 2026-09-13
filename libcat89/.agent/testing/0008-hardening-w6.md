# libcat89 testing scenarios (BDD) - W6 hardening continuation

## 0008-hardening-w6.md

The finite-builder and shape allocator-failure campaigns are green and have fixed
real failure-path leaks. This file records the REMAINING W6 scope: extending the
allocator-failure campaign across the generic wrapper-category and derived-
structure constructors, plus a callback-failure matrix and the no-status-retag
regression, all under ASan+UBSan+LSan.

- SCENARIO wrapper constructor atomicity: GIVEN valid inputs (base categories,
  functors, equalities) for a wrapper/derived constructor WHEN the constructor
  runs under an allocator that fails exactly one allocation (swept across every
  allocation point) THEN on failure it returns `CAT89_NOMEM` with `*out == NULL`
  and leaves no live allocation or partial object, and once past the last
  allocation it succeeds and releases cleanly.
- SCENARIO covered constructors: GIVEN the opposite, product, functor (new and
  identity), nat (new and identity), iso (new and identity), split mono/epi (new
  and identity), cone/cocone, limit/colimit, arrow, slice, coslice and comma
  constructors WHEN each is swept under the faulting allocator THEN every one is
  error-atomic (the above) and the sanitizer reports no leak.
- SCENARIO callback failure propagates verbatim: GIVEN a mock backend whose
  dom/cod/compose/identity/retain callback is programmed to fail with
  `CAT89_NOMEM` or `CAT89_CALLBACK` WHEN a derived operation (composition through
  a wrapper category, dom/cod, identity) triggers that callback THEN the status
  observed by the caller is exactly the injected status, never retagged.
- SCENARIO no-status-retag regression: GIVEN a mock whose compose fails with
  `CAT89_NOMEM` WHEN composition is invoked through the opposite wrapper (which
  delegates compose to the base backend) THEN the caller observes `CAT89_NOMEM`,
  not `CAT89_CALLBACK`.
- SCENARIO callback-failure matrix leaves no leak: GIVEN each mock callback
  failure mode GIVEN the failing operation is invoked GIVEN the returned handle
  is released/discarded appropriately THEN no live allocation remains under the
  sanitizer.
