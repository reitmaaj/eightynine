# Acceptance: iterative machine — decommission legacy (S2.6)

*Relates to `.agent/testing/0024-iter-decommission.md`. Stakeholder value:
story 0006. Design: `.agent/design/0007` §2, §6.*

## MUST

* Every wasm **function** invocation (`w89_invoke`, start functions) MUST
  always run through the iterative driver (level stack + unified value
  stack); there MUST be no selectable legacy recursive stepper and no
  `W89_ITER` env switch.
* A depth-5000 non-tail recursion MUST run with a flat native C stack (no
  `step_frame`/`step_label`/`step_handler` C-recursion) and MUST NOT
  overflow the stack.
* The former deep-recursion/perf STALL conformance files recorded in
  `.agent/tmp/*_sweep.log` (binary, call, call_indirect, fac, func_ptrs,
  imports, names, return_call, return_call_indirect, return_call_ref,
  skip-stack-guard-page, start) MUST each pass with zero FAIL/zero STALL in
  the full sweep.
* The full 257-file sweep MUST show zero FAIL and zero STALL, with skips
  only for by-design-excluded feature modules.
* `just ci` (lint + ob89 + test + diff-sweep) MUST be green.
* Instantiation's *constant/element-expression* evaluation (global/data/elem
  offset and element-expression initialisers) MUST run on the same iterative
  driver, as bounded single value/ref expressions with no control flow,
  calls or frames (flat, bounded native C stack).
* An active element segment carrying multiple element expressions, where any
  slot is a `ref.func`, MUST evaluate every such slot to a **non-null**
  function reference regardless of slot position; a `ref.null func` slot
  MUST evaluate to null. A `ref.func` slot MUST NOT come back null.
* There MUST be exactly one wasm evaluation path. The legacy C-recursive
  evaluator (`w89_eval`) MUST be fully deleted — not reachable from
  const/elem evaluation, function invocation, or unit tests.

*Note on scope:* "instantiation initialisation" here means the execution of
wasm functions during instantiation (notably the start function, via
`w89_invoke`), plus instantiation's *constant/element-expression* evaluation
(global/data/elem offset and element-expression initialisers). After the
full decommission, const/elem runs on the iterative driver as bounded flat
single value/ref expressions containing no control flow, calls or frames;
there is no separate legacy recursive stepper at all.

## MUST NOT

* The decommission MUST NOT change evaluation results, trap behaviour,
  exception propagation, exhaustion depth, or multi-value results relative
  to the (now removed) legacy path and the reference.
* It MUST NOT reintroduce native-stack growth per nesting level, or any
  per-instruction ancestor re-descend that makes recursion O(N^2).
* It MUST NOT leave dead legacy code, staging fallbacks, or the `W89_ITER`
  switch reachable. In particular the legacy recursive evaluator
  (`w89_eval`) MUST NOT remain reachable from any path, including
  const/elem evaluation and unit-test parity oracles.
* It MUST NOT evaluate an element-expression `ref.func` to a null reference;
  that would be an unacceptable silent corruption of a declared function
  binding.
* It MUST NOT add any skip to the sweep beyond by-design-excluded features.

## Gate

* Full 257-file conformance sweep: zero FAIL, zero STALL.
* `just ci` green.

## Note

* `return_call_indirect.wast:303` is a separately-tracked defect (design
  `0007` §7) — the acceptance here is that it must not STALL; if it is a
  genuine semantic/behavioural defect rather than a perf STALL, it is
  tracked and fixed separately rather than silently accepted.
