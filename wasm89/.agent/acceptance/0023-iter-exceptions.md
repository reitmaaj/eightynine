# Acceptance: iterative machine — exceptions (S2.5)

*Relates to `.agent/testing/0023-iter-exceptions.md`. Stakeholder value:
story 0006. Design: `.agent/design/0007` §2.2, §2.3, §6. Cross-reference:
exceptions acceptance `0010`/`0017`/`0018`.*

## MUST

* A `try_table` under the iterative driver MUST push a `BLOCK`-like level
  carrying its catch arms, with correct `end`/`exit_arity`/`base`.
* A `try_table` body that completes normally MUST be popped and splice its
  `exit_arity` results above its `base` exactly like an ordinary block.
* `throw`/`throw_ref` MUST unwind open levels to the nearest matching
  catch; on a match the catch's continuation MUST run with the exception
  payload delivered per the catch arm.
* An unmatched throw MUST surface as an exception result, identical to the
  legacy default.
* Throw/unwind outcome and payload MUST be identical under `W89_ITER=1` and
  the legacy default.
* Catch-arm payload values MUST be delivered in the exact specified order.

## MUST NOT

* Unwinding MUST NOT stop at a non-matching level or skip an inner matching
  catch in favour of an outer one.
* A normal `try_table` completion MUST NOT be mistaken for a catch, and a
  caught throw MUST NOT splice results as if the body completed normally.
* The throw MUST NOT corrupt the value/ref/exn payload, the unified value
  stack watermark, or open sibling levels.
* The migration MUST NOT regress `throw`/`throw_ref`/`try_table`
  conformance under the legacy default.

## Gate

* `throw`/`throw_ref`/`try_table` conformance green under `W89_ITER=1` and
  default, identical outcomes and payloads.
* `just lint ob89 test` green.
