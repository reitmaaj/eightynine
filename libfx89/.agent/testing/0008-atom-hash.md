# BDD scenarios: hash atom interning (D6)

These scenarios drive bucketed interning of parameterized atoms keyed by the
atom-domain `hash`, replacing the linear scan. Semantics are unchanged.

## Equivalence preserved

- SCENARIO Intern by equivalence: GIVEN two parameterized atoms with equal
  parameters WHEN created THEN the second reuses the first; the scan is
  restricted to the matching hash bucket and still decides by `compare`.
- SCENARIO Distinct atoms remain distinct: GIVEN parameterized atoms with
  unequal parameters in the same kind WHEN created THEN each is distinct even
  if their hashes collide.
- SCENARIO Domain without hash: GIVEN an atom domain with `hash == NULL` THEN
  parameterized atoms still intern correctly (bucket keyed by kind).

## Non-goals

- Nominal-atom interning (one per kind) is unchanged.
- Host `hash` must remain stable for a kind over the context lifetime, matching
  the documented `compare` total order.
