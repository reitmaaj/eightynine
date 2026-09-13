# Testing: iterative machine — decommission legacy (S2.6)

*BDD scenarios for removing the legacy C-recursive stepper and making the
iterative driver the sole evaluation path (PLAN S2.6). Stakeholder value:
story 0006. Design: `.agent/design/0007` §2, §6. Regression set: the
deep-recursion/perf STALL files recorded in `.agent/tmp/*_sweep.log`.*

## ITD-001 The iterative driver is the only path

SCENARIO: Legacy driver is removed
GIVEN the completed iterative machine
WHEN a wasm function is invoked (or a start function runs)
THEN it always runs through the iterative driver; there is no selectable
    legacy recursive stepper and no `W89_ITER` switch
AND every wasm function evaluation (invoke, start) uses the unified
    level stack and value stack.
AND instantiation's constant/element-expression evaluation runs through
    the same iterative driver as a bounded single expression (no control
    flow, calls or frames), so the native C stack stays flat and bounded
    there too, and the legacy recursive stepper is gone entirely (it is no
    longer reachable even for const/elem or as a unit-test oracle).

## ITD-004 Const/elem element expressions evaluate true refs on the driver

SCENARIO: ref.func element expression resolves under the driver
GIVEN an active element segment carrying more than one element expression
    (e.g. a funcref segment whose slots are a mix of `ref.func` and
    `ref.null func`)
WHEN instantiation evaluates each element slot on the iterative driver
THEN every `ref.func` slot yields a non-null reference to the declared
    function (never a null ref), regardless of slot position within the
    multi-expression segment.

SCENARIO: const offset global.get resolves under the driver
GIVEN a const expression used for a global/data/elem offset
WHEN instantiation evaluates it on the iterative driver
THEN it yields the same single value/ref the reference yields.

## ITD-005 The legacy stepper is fully deleted

SCENARIO: No legacy C-recursive evaluator remains
GIVEN the completed decommission
WHEN the source tree is inspected
THEN no C-recursive function-body evaluator (`w89_eval`) remains reachable
    from instantiation const/elem, function invocation, or unit tests;
    there is exactly one wasm evaluation path.

SCENARIO: Flat native stack under deep recursion
GIVEN a non-tail recursion of depth 5000
WHEN it runs repeatedly
THEN the native C stack stays flat (no `step_frame`/`step_label`/
    `step_handler` C-recursion), with no stack-overflow crash.

## ITD-002 The deep-recursion STALLs are gone

SCENARIO: Former STALL files complete
GIVEN the conformance files that previously STALLed on deep
    recursion/perf (e.g. `binary`, `call`, `call_indirect`, `fac`,
    `func_ptrs`, `imports`, `names`, `return_call`, `return_call_indirect`,
    `return_call_ref`, `skip-stack-guard-page`, `start` — see
    `.agent/tmp/*_sweep.log`)
WHEN the full 257-file sweep runs under the decommissioned iterative build
THEN every one of those files passes with zero FAIL and zero STALL.

## ITD-003 Full-sweep parity

SCENARIO: No semantic regression across the whole suite
GIVEN the full 257-file conformance sweep
WHEN run on the decommissioned build
THEN the aggregate shows zero FAIL and zero STALL with no skip beyond the
    by-design-excluded feature modules
AND `just ci` is green.
