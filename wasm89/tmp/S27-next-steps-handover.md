# S2.7 — next-steps handover (after S2.6 decommission LANDED)

Status owner: this file. Written after S2.6 (iterative-machine decommission)
LANDED on the working branch `feat/s2.6-iter-decommission` (three commits on
top of the S2.6 pre-decommission handover `tmp/S26-iter-decommission-handover.md`,
which is now superseded by that milestone's LANDED note). Records the verified
state, the one open loose end, and the recommended next steps for a fresh
session.

## Verified state at handover (branch tip `9f59528`)

- Full 257-file conformance: **37171 passed / 0 failed / 0 STALL** with the
  iterative driver as the sole wasm-function evaluation path.
  `.agent/tmp/s26_final_sweep.log` records the run.
- `just ci` (lint + ob89 + test + diff-sweep): exit 0; diff-sweep
  **3888 passed / 0 failed** vs wasmtime + wasm-interp.
- `down(5000)` perf gate: ~17 ms, flat native C stack.
- The iterative driver now handles param-typed `block/loop/if/try_table`,
  `table.get/set`, the `0xFC` bulk/table/saturating family, `0xD0..0xD4` ref
  leaves, `br_on_null`/`br_on_non_null`, and `call_ref`/`return_call_ref`.
- The `W89_ITER` env switch, the `iter_invoke_safe`/`iter_reach` eligibility
  scan, the `w89_eval_iter` legacy fallback, and the temporary `W89_COVERAGE`
  diagnostic are removed.
- `main` is untouched and green at `bee7256`. The working branch is **NOT
  merged**.

## One open loose end (recommended to close next)

The legacy C-recursive evaluator (`w89_eval`) was **not fully deleted**. It
remains reachable only from:
1. instantiation const/element-expression evaluation (`eval_const` at
   `instantiate.c:401`, `eval_elem_exprs` at `instantiate.c:794`), and
2. unit-test reference-oracle uses (e.g. `test_iter_direct_call_parity`).

Const/elem expressions are flat single value/ref expressions with no control
flow, calls, or frames, so they never C-recurse and add no native-stack risk.
The reason they were kept on `w89_eval`: routing them through the flat-stack
driver exposed a subtle mis-evaluation — a standalone `ref.func` element
expression returned a **null** ref (not the function) under the driver, for a
function inside a multi-expr active element segment (repro: `elem.wast:175`,
slot 7). The pure code path in `eval.c` `case 0xD2` looks deterministic, so
the cause was not pinned down by reading; a focused instrumented probe of that
case under the driver's standalone element-eval would find it. Fixing it would
let const/elem eval move to the flat-stack path and `w89_eval` be deleted
entirely (a fuller decommission).

## Recommended next steps (in order)

1. **Merge S2.6 to `main`.** `feat/s2.6-iter-decommission` is green and
   LANDED but not merged. With permission: merge to `main`, run the final
   post-merge gate (`just ci` + full 257-file sweep), delete the working
   branch. This is the clean checkpoint S2.6 was built toward. (Required:
   do not push/merge to `main` without explicit user permission.)

2. **Close the const/elem driver `ref.func` bug (decision pending).**
   - Fix the driver so instantiation const/element expressions run on the
     flat-stack path too, enabling deletion of the legacy `w89_eval` (fuller
     decommission). Probe `case 0xD2` under standalone element-eval to find
     why it returns null; add a reproduction test before fixing.
   - Or accept the current documented scope (acceptance/testing 0024 allow
     bounded single-expression eval for const/elem and the unit oracle).
   - Recommendation: fix it, since it is the one piece keeping legacy code
     reachable.

3. **Roadmap hygiene.** Fold the PLAN.md feature→conformance checklist items
   for S2.6 (already partly done); confirm acceptance/testing 0024 match
   shipped behaviour; re-check PLAN §4/S5 open items (`wip/wid-host-module`
   fate, wasmer as a 4th differential oracle).

4. **Continue the roadmap (optional next milestone).** PLAN's remaining
   PENDING streams: **S3.1 SIMD/v128**, **S3.2 GC + deep function-references**,
   optionally S1 differential breadth. S2.6 removed the O(N^2) fallback, making
   full-sweep runs cheap. Acceptance-first corpora already exist
   (`.agent/testing+acceptance/0025..0033`). S3.2 would naturally subsume the
   `ref.func`-in-elem and `call_ref` machinery already on the driver.
