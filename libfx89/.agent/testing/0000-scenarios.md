# BDD scenarios

These scenarios drive the milestone slices. IDs cross-reference
`test/` programs and the acceptance document.

## Context, kinds, operations

- SCENARIO Create/destroy: GIVEN no context WHEN `fx_ctx_new` runs THEN a
  context is returned; `fx_ctx_free` destroys it. `fx_ctx_free(NULL)` does
  nothing.
- SCENARIO Isolation: GIVEN two contexts WHEN the same effect name is defined
  in both THEN each receives its own id and no state crosses contexts.
- SCENARIO Kind ids: GIVEN kinds A,B,C defined in order WHEN ids are read
  THEN 0 is never returned and id(A) < id(B) < id(C).
- SCENARIO Kind name: GIVEN a kind named "IO" WHEN looked up THEN the id
  matches; an unknown name yields unknown; a duplicate name is rejected.
- SCENARIO Names copied: GIVEN a kind defined from a mutable buffer WHEN the
  buffer is changed THEN the stored name is unchanged.
- SCENARIO Op metadata: GIVEN operations File.read and File.write THEN each
  exposes its kind, name, and exact userdata; File.read and Socket.read may
  coexist while File.read twice is rejected.

## Atoms and rows

- SCENARIO Nominal atom: GIVEN a kind WHEN a nominal atom is created THEN its
  kind matches, it is not parameterized, and two creations of the same kind
  compare equal.
- SCENARIO Row construction: GIVEN atoms WHEN `fx_row_empty`, `fx_row_closed`,
  and `fx_row_var` build rows THEN atom count, ordering, openness, and tail
  match their descriptions.
- SCENARIO Canonicalize: GIVEN closed input `C,A,A,B` WHEN normalized THEN the
  row is `{A,B,C}` (unique, canonically ordered).
- SCENARIO Disjoint tail: GIVEN open row `{A,B|e}` WHEN queried THEN
  `A not-in e` and `B not-in e` are forced.
- SCENARIO Tail duplicate rejected: GIVEN `{A|e}` WHEN `A in e` is required
  THEN solving reports a contradiction.
- SCENARIO Membership: GIVEN `{A,B}` THEN `A in` is true, `C in` is false,
  and for `{A,B|e}`, `C in` is UNKNOWN until resolved.
- SCENARIO Extend: GIVEN `{A|e}` extended with B THEN the row is `{A,B|e}`.

## Equality

- SCENARIO Closed equality: GIVEN `{A,B} = {B,A}` THEN it succeeds; `{A} =
  {B}` fails; `{A} = {A,B}` fails.
- SCENARIO Bind: GIVEN `e = {A,B}` THEN `e` resolves to that exact closed
  row; alias `e1 = e2` makes a fact on `e1` hold on `e2`.
- SCENARIO Open-to-closed: GIVEN `{A|e} = {A,B}` THEN `e = {B}`; GIVEN
  `{A,C|e} = {A,B}` THEN it fails.
- SCENARIO Open/open: GIVEN `{A|e1} = {B|e2}` THEN e1 = {B|e3}, e2 = {A|e3}
  for a fresh e3 and both sides normalize identically.
- SCENARIO Shared tail: GIVEN `{A|e} = {B|e}` THEN it fails; `{A,B|e} =
  {B,A|e}` succeeds.
- SCENARIO Occurs: GIVEN `e = {A|e}` or an indirect `e1 = {A|e2}, e2 = {B|e1}`
  THEN solving fails as occurs.

## Membership / lacks / purity

- SCENARIO Facts: GIVEN `A in e` THEN it is satisfied; GIVEN `A not-in e`
  THEN membership is false; GIVEN both THEN solving fails.
- SCENARIO Idempotence: GIVEN the same member or lacks requirement repeated
  THEN no contradiction or duplicate semantic state arises.
- SCENARIO Member on closed: GIVEN `C in {A,B}` THEN it fails.
- SCENARIO Lacks: GIVEN `C not-in {A,B}` THEN satisfied; GIVEN `A not-in
  {A,B}` THEN it fails.
- SCENARIO Pure: GIVEN `require_pure(e)` THEN `e` binds to `{}`; GIVEN a known
  member on `e` with require_pure THEN it fails.

## Subset

- SCENARIO Closed subset: GIVEN `{A} subset {A,B}` THEN true; GIVEN `{A,C}
  subset {A,B}` THEN false.
- SCENARIO Propagate: GIVEN `{A,B} subset {A|e}` THEN `B in e` is derived.
- SCENARIO Residual forward: GIVEN `e1 subset e2` and later `A in e1` THEN
  `A in e2` (including when added after a first solve).
- SCENARIO Residual backward: GIVEN `e1 subset e2` and `A not-in e2` THEN
  `A not-in e1` is derived.
- SCENARIO Upper bound: GIVEN `e subset {A,B}` THEN `A in e` holds but `C in
  e` fails, with no enumeration of the atom universe.
- SCENARIO Transitivity: GIVEN `e1 subset e2 subset e3` and `A in e1` THEN
  `A in e3`; dually for absence.

## Join

- SCENARIO Closed join: GIVEN `fx_row_union({A,B},{B,C})` THEN the result is
  `{A,B,C}`; union with `{}` and with itself are identities.
- SCENARIO Forward: GIVEN `o = left union right` and `A in left` THEN `A in o`.
- SCENARIO Negative: GIVEN `A not-in o` for `o = l union r` THEN `A not-in l`
  and `A not-in r`.
- SCENARIO Reverse forced: GIVEN `A in o` and `A not-in l` THEN `A in r`.
- SCENARIO Underdetermined: GIVEN only `A in o` for `o = l union r` THEN
  `A in l` and `A in r` both stay UNKNOWN and each of the three concrete
  placements remains satisfiable.

## Removal / free vars / closedness

- SCENARIO Remove: GIVEN `fx_row_remove({A,B},A)` THEN `{B}`; removing an
  absent atom leaves the row unchanged.
- SCENARIO Unsupported removal: GIVEN a symbolic-tail row where exact
  difference is unresolved THEN `fx_row_remove` reports unsupported rather
  than guessing.
- SCENARIO Closedness: GIVEN a closed row THEN is_closed is true; GIVEN an
  unresolved open row THEN UNKNOWN, and the query never binds the tail.
- SCENARIO Free vars: GIVEN `{A|e1}` and `{B|e2}` rows THEN free-variable
  enumeration returns each variable once in deterministic order.

## Schemes

- SCENARIO Generalize: GIVEN body `{A|e}` with no exclusions THEN the scheme
  quantifies `e`; with `e` marked nongeneralizable THEN it does not.
- SCENARIO Instantiate fresh: GIVEN one scheme instantiated twice THEN the two
  results use distinct fresh variables and are analogous modulo renaming.
- SCENARIO Residual capture: GIVEN symbolic subset/join state generalized
  THEN the scheme retains the residual relation, and instantiation recreates
  live constraints that propagate again.
- SCENARIO Invalid scheme: GIVEN an explicit scheme whose captured constraint
  references a mutable unquantified outer variable THEN construction is
  rejected.

## Checkpoints

- SCENARIO Rollback facts/bindings: GIVEN a checkpoint THEN facts, bindings,
  aliases, constraints, and join/subset residuals added afterward disappear on
  rollback, while kinds and operations survive.
- SCENARIO Nesting: GIVEN cp1 then cp2 THEN rollback cp2 then commit cp1
  succeeds; rolling back a non-top checkpoint is rejected.
- SCENARIO Commit: GIVEN facts committed THEN they remain and a later rollback
  to the committed token is rejected.

## Diagnostics

- SCENARIO Conflict kinds: GIVEN closed mismatch THEN equality conflict; a
  member+lacks clash is a member/lacks conflict; a cycle is an occurs
  conflict; and the reported constraints carry the expected host userdata.
