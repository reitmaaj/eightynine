# 0004 — generated and exhaustive scenarios

## Exhaustive machines

SCENARIO complete small-machine enumeration
  GIVEN every deterministic machine with `N` states and `E` events where each
    `(state, event)` cell is absent or one of the `N` destinations
  WHEN each machine is validated, started, stepped for every pair, and queried
    for acceptance
  THEN every result matches the independent reference model

SCENARIO default sweep
  GIVEN `N` in 1..3 and `E` in 1..2
  WHEN the generated suite runs
  THEN all `(N+1)^(N*E)` machines are covered

SCENARIO deep sweep
  GIVEN `N == 3` and `E == 3`
  WHEN the deep generated suite runs
  THEN all `4^9 == 262144` machines are covered

SCENARIO table reordering under generation
  GIVEN a generated valid machine
  WHEN its state and edge tables are permuted
  THEN every result is unchanged

SCENARIO generated renaming
  GIVEN a generated valid machine
  WHEN state, event, and effect IDs are renumbered bijectively
  THEN the renamed behavior is the image of the original behavior

## Generated invalid machines

SCENARIO single-invariant mutation
  GIVEN a valid generated machine
  WHEN exactly one invariant is broken (duplicate state, missing initial,
    missing source, missing destination, duplicate edge key, NULL nonempty
    span)
  THEN the corresponding validation error is returned

SCENARIO multiple defects
  GIVEN a machine with defects in several validation stages
  WHEN `fsm89_validate` is called
  THEN the earliest stage in the documented order is reported
