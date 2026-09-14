# 0002 — libsyntax89 test contract

The test contract makes the public contract rigid while leaving implementation
freedom. It is enforced by the artifacts named in
`.agent/testing/0004-invariants-and-test-matrix.md`.

## Must hold for every test binary

1. One smoke test proves the end-to-end path before any other suite runs.
2. Every public function is named by at least one test (`just api-coverage`).
3. `syntax89_test_check` runs after every successful mutator in unit, model,
   and generated tests.
4. Every failed mutator is wrapped in the snapshot harness and must leave
   state, root, counts, node metadata, and edge order unchanged.
5. Every allocating operation is exercised with the fault allocator at every
   allocation point; the graph stays logically unchanged, the error is
   `SYNTAX89_ENOMEM`, and `live` returns to zero after destroy.
6. `validate(g) == OK` implies `freeze(g)` is `OK` or `ENOMEM`;
   `validate(g) == ECYCLE` implies `freeze(g) == ECYCLE`;
   `validate(g) == EGRAPH` implies `freeze(g) == EGRAPH`.
7. Every documented error code is produced by at least one test, and no
   function returns an undocumented code.
8. Validation, traversal, and construction are deterministic for identical
   insertion sequences.

## Must reject

- `add_node` rejects `begin > end` with `EINVAL` and does not allocate.
- Unknown ids and `SYNTAX89_ID_NONE` are rejected with `ENODE`.
- Out-of-range child indexes are `EINVAL`; iterator exhaustion is `END`.
- A missing or unknown root freezes with `EGRAPH`.
- A reachable cycle freezes with `ECYCLE`; an unreachable node freezes with
  `EGRAPH`.
- Mutators on a FROZEN graph return `ESTATE` and change nothing.
- Count and byte-size overflow returns `EOVERFLOW` with no allocation.
- Walks reject an unknown root with `ENODE` and a cyclic region with `ECYCLE`.
- A callback returning a positive value other than `END` aborts with
  `EINVAL`.

## What the tests must not freeze

Tests must not depend on exact capacities, growth factors, allocation counts
outside the fault harness, memory addresses, internal storage order beyond
public child order, validation traversal order, or any hash layout. The
contract is logical; implementation details stay free.
