# BDD scenarios: correctness pass

These scenarios drive the P0/P1 correctness fixes (exact JOIN with closed
inputs, transactional rollback, checkpoint OOM atomicity, OOM propagation,
three-valued purity/query proofs). IDs cross-reference `test/` programs and
the acceptance document.

## Exact JOIN with a closed output side

- SCENARIO Closed-input open output success: GIVEN `z` open and
  `{A | z} = {A} \u222a {B}` WHEN solved THEN the equality holds with `z`
  bound to `{B}` (the output's explicit head is respected, not dropped).
- SCENARIO Closed-input open output mismatch: GIVEN `{C | z} = {A} \u222a
  {B}` WHEN solved THEN it fails; `z` must not be silently bound so that the
  result becomes `{A,B,C}`.
- SCENARIO Closed-input closed output: GIVEN `{A,B} = {A} \u222a {B}` WHEN
  solved THEN it succeeds and both sides normalize to `{A,B}`.

## Transactional rollback of solver state

- SCENARIO Rollback restores constraint progress: GIVEN `A in e` then a
  checkpoint then a solve that satisfies it (so the member constraint is
  SATISFIED) THEN on rollback the solved constraint state and the derived
  fact both disappear, and a later `fx_solve()` re-triggers the propagation.
- SCENARIO Rollback re-derives: GIVEN rollback above THEN `A in e` requires
  the fact again, `membership(e,A)` is no longer UNKNOWN-true, and no stale
  SATISFIED constraint silently skips propagation.
- SCENARIO Checkpoint OOM is atomic: GIVEN a nonzero number of variables WHEN
  snapshot allocation would fail THEN the checkpoint is not pushed and
  rollback/commit of that token is not possible, rather than returning a token
  that dereferences a null snapshot.

## OOM does not change answers

- SCENARIO Set difference OOM: GIVEN a set difference that legitimately
  equals the empty set WHEN allocation fails THEN the failure is reported
  (NOMEM) instead of being treated as an empty result.
- SCENARIO Join support OOM: GIVEN JOIN support collection WHEN allocation
  fails THEN `FX_ERR_NOMEM` propagates instead of silently truncating the
  support/union.

## Three-valued purity and query proofs

- SCENARIO Purity with required fact: GIVEN `A in e` (so `e` requires `A`)
  THEN `fx_row_is_pure(e)` returns `FX_FALSE`, not `FX_UNKNOWN`.
- SCENARIO Subset reflexive open: GIVEN an unresolved open row `R` THEN
  `fx_check_subset(R,R)` returns `FX_TRUE`.
