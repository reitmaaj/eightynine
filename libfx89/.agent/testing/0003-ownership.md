# BDD scenarios: ownership and contract hardening

These scenarios drive cross-context object rejection, atom-domain
validation/copying, and representational-overflow guards.

## Cross-context ownership

- SCENARIO Foreign row rejected: GIVEN a row created in another context WHEN
  it is used in a `fx_require_*` constraint in this context THEN
  `FX_ERR_INVALID` is returned.
- SCENARIO Foreign atom rejected: GIVEN an atom created in another context
  WHEN it is passed to a member/lacks requirement or compared via
  `fx_atom_equal` in this context THEN it is refused/unequal, never compared
  through a foreign atom-domain callback.

## Atom domain installation

- SCENARIO Incomplete domain rejected: GIVEN an atom domain missing its
  `compare`, `copy`, or `destroy` callback WHEN installed THEN
  `FX_ERR_INVALID` is returned.
- SCENARIO Domain copied by value: GIVEN a domain installed from a caller
  struct THEN the context retains a private copy, so later caller mutation of
  the original does not change the domain used for parameterized atoms.

## Representational overflow

- SCENARIO Allocation size overflow: GIVEN a count whose byte-size product
  would wrap WHEN allocated THEN allocation fails safely (NOMEM) instead of
  wrapping to a small size.
