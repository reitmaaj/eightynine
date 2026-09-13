# Acceptance: iterative machine — tail calls (S2.4)

*Relates to `.agent/testing/0022-iter-tailcalls.md`. Stakeholder value:
story 0006. Design: `.agent/design/0007` §2.2, §4.*

## MUST

* `return_call`/`return_call_indirect` MUST replace the current `FUNC`
  level with the callee `FUNC`, reusing the same value frame/base so the
  level-stack depth does not grow.
* A tail transition MUST be uncharged against the exhaustion budget (only
  non-tail `FUNC` pushes are charged).
* A 1,000,000-iteration `return_call` loop MUST complete successfully
  without exhaustion.
* Tail-call results MUST be identical under `W89_ITER=1` and the legacy
  default.
* Callee params MUST be set up from the caller's tail arguments above the
  frame base and replaced by the callee's `exit_arity` results on return,
  leaving caller values below the base untouched.

## MUST NOT

* A tail call MUST NOT grow the level stack, charge the budget, or leak a
  stale `FUNC` level.
* A tail call MUST NOT consume or corrupt caller values below the frame
  base.
* The migration MUST NOT change the depth/behaviour of `assert_exhaustion`
  for genuinely unbounded non-tail recursion.
* It MUST NOT regress the legacy default path or the `just test` gate.

## Gate

* `return_call*` conformance green under `W89_ITER=1` and default, identical
  outcomes (1M tail loops must still succeed, uncharged).
* `just lint ob89 test` green.
