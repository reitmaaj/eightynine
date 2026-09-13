# libcat89 acceptance tests - correctness-closure increment

## 0002-correctness-gate.md

The library MUST exhibit the behavior listed under "must exhibit" and MUST
reject, avoid, refuse, or fail safely on the behavior listed under "must
reject". Every rejection must leave outputs NULL/zero and must not enter a
representation-specific callback after a provenance rejection.

## Must exhibit

- `cat89_functor_compose(G, F)` reports `source(F) -> target(G)` and maps
  through `F` then `G` (distinct `C`, `D`, `E`).
- `owns_obj`/`owns_mor` for every built-in backend: accept own handles, reject
  NULL and foreign handles, return exactly 0 or 1, and are safe on live foreign
  handles (no dereference/inspect/retain/release) under ASan/UBSan.
- Core wrappers (`dom`, `cod`, `identity`, `compose`, `obj_same`, retain,
  release) enforce ownership before representation callbacks; `compose` checks
  both operands before `cod`/`dom`.
- Every public raw-handle API enforces known provenance before using,
  retaining, storing, comparing, mapping or casting the handle.
- Every typed capability mismatch (`eq`/`enum`/`shape_enum` bound to another
  category instance) returns `CAT89_INVALID` before any effect callback.
- Generic algorithms use `cat89_obj_same` (not C pointer equality) for object
  identity, so distinct-but-equal handles behave correctly in compose, finders,
  exhaustive checks and law checks.
- Finite builder mutations are failure-atomic; `cat89_finite_validate`
  recognizes exactly valid finite categories; `cat89_finite_build` validates,
  never consumes the builder, and rejects invalid tables with `CAT89_INVALID`.
- Checked arithmetic prevents size/refcount overflow without mutation or
  out-of-bounds allocation.
- Finder object collection is single-pass; factorization rejects candidates
  over another diagram; finder factors return `CAT89_OK` + owned mediator or
  `CAT89_NOT_FOUND` + NULL.
- `dom`/`cod`/`identity`/`compose` results are owned by the category that
  produced them; retain does not duplicate provenance; final release removes
  the handle from the live registry.
- The complete suite passes under GCC and Clang, under ASan/UBSan/LSan, and
  under the allocation-failure sweep.

## Must reject

- A category whose `owns_obj` or `owns_mor` operation is NULL.
- Foreign object/morphism handles at every public raw-handle API (return
  `CAT89_INVALID`, boolean outputs 0, outputs NULL; no callback runs).
- NULL/foreign handle accessor calls (return NULL).
- Capability/structure combinations whose category instances disagree
  (`CAT89_INVALID` before equality/enumeration/factor callbacks).
- A finite table with a missing/mistyped identity, missing/non-composable/
  mistyped composition row, duplicate composition key, or broken left/right
  identity/associativity (`*out_valid == 0`; build `CAT89_INVALID`).
- Allocation failure at any builder mutation or build point (`CAT89_NOMEM`,
  builder unchanged, outputs NULL, no partial row/object published).
- Size/refcount overflow (non-OK, state unchanged, no wrapped allocation).
- A limit/colimit candidate cone/cocone over a different diagram.
- Finder factorization with zero or multiple mediators (`CAT89_NOT_FOUND` +
  NULL, never `CAT89_OK` + NULL).
- `cat89_finite_build` on an invalid table must not publish, release or mutate
  the builder.

## Test-private instrumentation

- `cat89_finite_build_unchecked`, `cat89_category_test_set_refs` and
  `cat89_finite_test_set_mor_refs` are internal (declared only in
  `src/cat89_internal.h`) and exist solely for malformed-law fixtures and
  overflow tests. No public corruption API is added.
