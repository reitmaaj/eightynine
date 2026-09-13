# S2.5 iterative exceptions unwinding — implementation handover

Status owner: this file. Companion to `tmp/PLAN.md` (§2.5) and
`tmp/S24-iter-tailcalls-handover.md`. This is the *post-implementation*
handover for S2.5, landed on `main` via the short-lived working branch
`feat/s2.5-iter-exceptions` (merged, `07fe63a`). It records state, what
changed, verification, and every defect/gap observed for S2.6 (decommission).

## Definition of done met

Acceptance `.agent/acceptance/0023-iter-exceptions.md` (MUST / MUST NOT) and
scenarios `.agent/testing/0023-iter-exceptions.md` (ITX-001/002/003) are
covered and green. `throw`/`throw_ref`/`try_table` conformance under
`W89_ITER=1` is byte-identical to the legacy default. Default path stays legacy
and unchanged.

## State at handover

- Merged to `main` (`07fe63a`; feature commit `600628b`). Legacy recursive
  stepper remains the default; `W89_ITER=1` selects the driver.
- The driver (`w89_eval_iter`) now also migrates exceptions on top of the S2.2
  block/`br`, S2.3 `call`/`call_indirect`/`return`, and S2.4
  `return_call`/`return_call_indirect` migrations. S2.6 (legacy decommission):
  NOT done.

## What changed (files)

- `src/eval.c`:
  - `iter_op_migrated` now also returns migrated for `0x08`(throw),
    `0x0A`(throw_ref) and `0x1F`(try_table), so bodies containing exceptions
    become driver-eligible (previously they routed entirely to legacy).
  - `iter_instr_ok` treats `0x1F` like a block opener, so a `try_table` with
    block-type params is conservatively routed to legacy (mirrors the
    block/loop/if policy). Reachable-closure check (`iter_reach`/
    `iter_invoke_safe`) unchanged.
  - In the driving loop `try_table`(0x1F) now enters the existing
    `enter_block` path as a `W89_LVL_BLOCK` whose level records
    `catches`/`ncatches` (fields were already reserved on `w89_lvl`).
  - New `do_throw`(0x08) and `do_throw_ref`(0x0A) paths take the thrown tag and
    payload (values for `throw`; the `exn`'s tag+args for `throw_ref`), then a
    `throw_scan` unwinds open levels innermost-out to the first `try_table`
    whose catch arms match (arm kind: 0 `catch`, 1 `catch_ref`, 2 `catch_all`,
    3 `catch_all_ref`), resolving the catch's tag in the try_table's declaring
    function frame.
  - `throw_hit` finalizes a match: frees every driver-owned `FUNC` frame above
    the target (restoring one budget unit per freed frame, mirroring
    `fn_splice`), truncates the shared value stack to the target level's base,
    delivers the payload per arm kind (`catch`: the tag params; `catch_ref`:
    params then an `exnref`; `catch_all`: nothing; `catch_all_ref`: one
    `exnref`), then resumes at the catch's branch target — an enclosing block
    (label `l` names the try_table's parent at `l` == 0) by popping to it, or an
    enclosing `FUNC` via the existing `fn_splice` return path.
  - An unmatched throw surfaces through a new `out_exception` label as
    `W89_EVAL_EXCEPTION` with the tag + payload, freeing frames and the value
    stack (identical to legacy `A_THROWING` surfacing).
  - Small pure helpers `thr_take_top` / `thr_copy_val` copy a throw payload off
    the shared value stack.
- `test/test_exn.c`: new `test_iter_catch_parity` (cross-function: a callee
  throws inside the caller's `catch_all`; runs the same program once on the
  legacy default and once with `W89_ITER=1` and asserts both return 23,
  exercising the driver's cross-frame frame-free/budget-restore unwind). Existing
  exception unit tests (`test_throw_*`, `test_try_catch_*`) already pass under
  `W89_ITER=1` unchanged (their bodies are now driver-eligible).
- Docs: `tmp/PLAN.md` §2.5 / checklist and `.agent/design/0007-iterative-machine.md`
  updated to LANDED. No `.agent/testing`/`.agent/acceptance` edit was needed:
  `0023` already encoded ITX-001/002/003.

## Verification performed

- `just lint ob89 test` green; ob89 clean on `src/*.c` (the new code follows the
  obligations model: every struct-field/array read is hoisted into a witness
  local before branching/calling/storing).
- Unit: `test_eval`, `test_calls`, `test_exn` (incl. new parity) all pass;
  `down(5000)` perf gate still green (~9–19 ms).
- Exceptions conformance under `W89_ITER=1` and under the legacy default are
  each **93 passed / 0 failed / 2 skipped** (throw, throw_ref, try_table) —
  identical.
- Control-flow/call regression subset under `W89_ITER=1`: `block`/`loop`/`call`/
  `fac` pass. (`if.wast:580` fails identically on pristine `main` `d4676d0`
  under `W89_ITER=1`, verified via a clean worktree build — a pre-existing S2.x
  driver defect, NOT an S2.5 regression, and out of scope here.)

## Defects and gaps observed (handover notes for S2.6+)

1. **Legacy still default; `W89_ITER=1` opts in.** S2.6 decommission must make
   the driver the sole path and delete the legacy recursive stepper +
   `W89_ITER` switch.
2. **Catch-target semantics is the branch-target of the enclosing block**:
   a catch clause's label `l` names the try_table's immediate parent block at
   `l == 0` (grandparent at `l == 1`, ...), i.e. a caught throw is a `br` to an
   enclosing structured block (or, defensively, a return through `fn_splice` if
   the target is a `FUNC` level). The driver models a `try_table` as one
   `BLOCK` level; there are no extra per-arm labels in this encoding (arms are
   external blocks named by the user), which is why `iter_br`-style popping
   sufficed.
3. **A `try_table` with block-type params is conservatively routed to legacy**
   (not yet migrated). Relax only if conformance requires it.
4. **Budget contract**: an exception unwind frees each abandoned driver-owned
   `FUNC` frame and restores one budget unit, so the budget never leaks across
   an unwound call chain. Verify in S2.6 that deep try/catch recursion that
   terminates below 5000 still succeeds and runaway recursion still traps at
   5000 (uncharged tail + bounded try nesting).
5. **The `if.wast:580` driver failure** (pre-existing) is a candidate to fix in
   S2.6 before the decommission sweep claims zero FAIL under the driver.
