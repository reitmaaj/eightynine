# 0001 — libfsm89 acceptance

Release acceptance depends on every statement below holding. Each maps to
deterministic tests plus the recipes named in `Justfile`.

| ID | Statement | Evidence |
| --- | --- | --- |
| A01 | Every public function has success and failure tests. | `test/unit/*`, `test/model/*`, `test/generated/*`; `scripts/api-coverage.sh` |
| A02 | Validation proves every documented structural invariant. | `test/unit/test_validate.c`, `test/generated/test_mutations.c` |
| A03 | Validation reports the documented first error when defects combine. | `test/unit/test_validate_order.c`, `test/generated/test_mutations.c` |
| A04 | A successful step returns `from`, `event`, `to`, and the three spans. | `test/unit/test_step.c` |
| A05 | Emission order is leave then edge then enter for ordinary and self transitions. | `test/unit/test_effects.c` |
| A06 | Self-transitions never suppress leave or enter. | `test/unit/test_effects.c` eight-way matrix |
| A07 | Startup returns the initial state and only its enter span. | `test/unit/test_start.c` |
| A08 | Missing transition and unknown state emit nothing and leave the result unchanged field-for-field. | `test/unit/test_missing.c` |
| A09 | `FSM89_ESTATE` and `FSM89_NO_TRANSITION` are never confused. | `test/unit/test_missing.c` |
| A10 | All returned spans are borrowed from the definition. | `test/unit/test_borrow.c` |
| A11 | No public operation mutates the definition. | `test/unit/test_immutability.c` |
| A12 | Table order carries no semantics. | `test/unit/test_order.c`, `test/generated/test_generated.c` |
| A13 | Renaming state, event, and effect IDs preserves behavior up to the bijection. | `test/unit/test_metamorphic.c` |
| A14 | IDs are never used as table indices; 0, `ULONG_MAX`, and sparse IDs work. | `test/unit/test_ids.c` |
| A15 | Unreachable states and accepting states with outgoing edges validate. | `test/unit/test_reachability.c` |
| A16 | `fsm89_accepting` depends only on the state definition. | `test/unit/test_accepting.c` |
| A17 | Two machines interleaved behave exactly as in isolation. | `test/unit/test_interleave.c` |
| A18 | Production behavior matches an independent reference model over hand fixtures and generated machines. | `test/model/*`, `test/generated/test_generated.c` |
| A19 | The library allocates nothing and references no I/O or threading symbols. | `scripts/audit.sh` |
| A20 | The exported symbol set is exactly the four public functions plus the three documented internal `fsm89__*` helpers. | `scripts/audit.sh` |
| A21 | Strict C89 and strict C23 builds run the same behavioral suite under GCC and Clang. | `just matrix` |
| A22 | ASan and UBSan runs are clean. | `just sanitize` |
| A23 | Every source line and branch is covered by the suite. | `just coverage` |
| A24 | The header compiles alone, tolerates double inclusion, and works from C++23. | `test/compile/*`, `test/cpp/header_check.cpp` |
| A25 | Result spans are not writable through the public API. | `test/compile/reject_write_through_result.c` |

## Unacceptable behavior (must reject or must not exhibit)

- Accepting a definition with duplicate state IDs, duplicate `(from, event)`
  edges, a missing initial state, a missing edge endpoint, a nonempty effect
  span with NULL storage, or a NULL definition.
- Rejecting a definition because it has unreachable states, accepting states
  with outgoing edges, non-accepting terminal states, empty effect spans, or
  identifier values 0 or `ULONG_MAX`.
- Writing any field of `*result` when `fsm89_step` returns
  `FSM89_NO_TRANSITION` or `FSM89_ESTATE`.
- Returning `FSM89_ESTATE` for a missing transition or
  `FSM89_NO_TRANSITION` for an unknown state.
- Mutating the definition, state table, edge table, or effect arrays from any
  public operation.
- Executing, interpreting, deduplicating, or reordering emitted effects.
- Suppressing leave or enter emissions on a self-transition.
- Emitting leave or edge effects during startup.
- Treating `accepting` as terminal or `terminal` as accepting.
- Allocating memory, performing I/O, calling callbacks, or reading or writing
  hidden global state.
- Depending on state, event, or effect numeric values as table positions.
- Letting table order affect any semantic result.
