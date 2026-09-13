# S2.7 — const/elem on the driver + full legacy deletion (LANDED)

Status owner: this file. Records the closure of the S2.6 loose end identified
in `tmp/S27-next-steps-handover.md`: instantiation const/element expressions
now run on the iterative driver, host functions are invoked as leaves, and
the legacy C-recursive evaluator `w89_eval` is fully deleted. Work landed on
branch `feat/s27-constelem-driver` (single commit `91bde44` on top of the
S2.6 `feat/s2.6-iter-decommission` tip `dc6b697`). `main` is untouched.

## Root cause of the const/elem ref.func bug

`eval_const`/`eval_elem_exprs` sliced one slot of a multi-expression active
element segment via `w89_code_range(..., start, len)`, which builds a per-slice
admin queue for the (then-)legacy `w89_eval`. The iterative driver ignores
that queue and seeds its outer FUNC level over `code->src[0..nsrc)` — the
**whole concatenated element-expression vector**. So every element slot
re-ran slot 0. When slot 0 is `ref.null func` (as in elem.wast's `(elem ...
funcref (ref.null func) (ref.func $f))` segment), every later `ref.func` slot
came back null, so `call_indirect` to slot 7 hit "uninitialized element 7"
(elem.wast:175). Not a `case 0xD2` defect at all — a slice-not-respected bug.

## Fix

- `instantiate.c`: new `eval_expr_driver(inst, items, start, len, out)` sets
  `code->src = items+start` and `code->nsrc = len` (the exact slot slice) and
  calls `w89_eval_iter`. `eval_const` and `eval_elem_exprs` route through it.
- `eval.c`: host invocation in `w89_invoke` no longer drives the legacy
  stepper; a host func is called directly as a leaf (`invoke_host_direct`),
  mirroring how the driver already calls hosts inside wasm bodies.
- Deleted `w89_eval` and its `eval.h` declaration. No source path references
  it. Internal admin-step helpers (`step_frame`/`step_label`/`step_handler`
  etc.) remain only because `w89_step`'s dispatch (used by the driver's leaf
  path) still references them; they are not a reachable evaluator loop.

## Oracle rework (chosen: fixed expectations)

The legacy-vs-driver parity unit oracles were rewritten to assert concrete
expected results on the driver alone:
- `test_eval.c`: `run()` now calls `w89_eval_iter`; `parity_eq` checks the
  driver reaches a definite (OK/TRAP/CRASH) status; the machine-stack
  legacy comparison asserts 16.
- `test_eval.c` `test_return`/`test_terminal`: bare-fragment `return` /
  escaping-`br` are, under the driver's function-framed model, clean function
  returns (matching S2.6 br-to-function behaviour), not "undefined frame/label"
  crashes.
- `test_calls.c` `test_iter_direct_call_parity`: asserts the driver forwards
  the value 9 through a real `call`.
- New module regression `test_elem_expr_ref_func_later_slot` in
  `test_instantiate.c`: builds the elem.wast:175-style active funcref segment
  (`ref.null func` at slot 6, `ref.func 0` at slot 7) and asserts
  `call_indirect` to slot 7 reaches the function (returns 65).

## Verification (all green on `91bde44`)

- `just ci` exit 0 (lint + ob89 + test + diff-sweep 3888/0).
- Full 257-file conformance sweep: **37171 passed / 0 failed / 0 STALL**
  (matches the S2.6 baseline exactly).
- `down(5000)` perf gate ~14-25 ms (flat native C stack).

## Docs updated

- `.agent/testing/0024`: ITD-004 (ref.func elem expr resolves under driver),
  ITD-005 (legacy stepper fully deleted).
- `.agent/acceptance/0024`: MUST/MUST-NOT for driver const/elem eval, non-null
  ref.func element slots, and full `w89_eval` deletion.
- `.agent/design/0007` §6 and `tmp/PLAN.md`: S2.7 LANDED status.

## Remaining roadmap

With the loose end closed, the S2.7 next-steps items are: (1) merge
`feat/s2.6-iter-decommission` (or this branch) to `main` (needs permission;
`main` is untouched and green at `bee7256`); (2) roadmap hygiene per PLAN;
(3) continue S3.1 SIMD / S3.2 GC. `main` has NOT been merged or pushed.
