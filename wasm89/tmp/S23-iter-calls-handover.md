# S2.3 iterative calls/frames — implementation handover

Status owner: this file. Companion to `tmp/PLAN.md` (roadmap), the planning
doc `tmp/S23-iter-calls-handoff.md`, and `.agent/design/0007` §2.3/§3/§4.
This is the *post-implementation* handover for S2.3, which is landed and
merged to `main`. It records state at handover, what changed, how it was
verified, and every defect/gap observed along the way for the next stages
(S2.4 tail calls, S2.5 exceptions, S2.6 decommission).

## Definition of done met

Acceptance `.agent/acceptance/0021-iter-calls.md` (ITC-001..006) and
scenarios `.agent/testing/0021-iter-calls.md` are covered and green.
`test/perf_on2.sh` is re-enabled in `test/run_tests.sh`, runs under
`W89_ITER=1`, and passes: `down(5000)` ~15 ms (was ~30 s O(N^2)), flat C
stack, normal return. Default path stays legacy and byte-identical.

## State at handover

- `main` @ `6b0ba85` (fast-forward from `076d27c`): the S2.3 changeset.
- Legacy recursive stepper remains the default; `W89_ITER=1` opt-in selects
  the driver through `w89_invoke`.
- Driver (`w89_eval_iter`, `src/eval.c`) now migrates `call`/`call_indirect`/
  `return` onto `W89_LVL_FUNC` levels in addition to the S2.2 block/`br`
  migration. S2.4 tail calls, S2.5 exceptions, S2.6 decommission: NOT done.

## What changed (files)

- `src/eval.h`: added `w89_config.resn` (top-level result arity: >0 truncates
  the level-0 `FUNC` results on exit; 0 keeps the harness "leftover" rule).
- `src/eval.c`:
  - `iter_op_migrated` now also returns migrated for `0x0F`(return),
    `0x10`(call), `0x11`(call_indirect).
  - Driver dispatch is per-top-level body (`tp->src/nsrc`) and per-function
    module types (`c->frame->inst->types`), so a callee body is stepped with
    its own instruction range/env after `c->frame` is swapped to the callee.
  - New helpers: `iter_instr_ok`/`iter_body_ok` (structural migratability of a
    body: all ops migrated, no block params, optional call_indirect rejection),
    `iter_reach`/`iter_has`/`iter_invoke_safe` (call-closure eligibility,
    host = leaf, visited-set over funcinst pointers), `iter_sync_frame`,
    `iter_free_frames`, `iter_func_of`.
  - Call handling (`do_call`): host runs inline (no level, per legacy
    `step_invoke`); wasm pushes a `FUNC` level — `frame_alloc`, args→locals,
    declared-locals init, `code->vsn = vsn-n1`, `lv.base = vsn-n1`, budget
    charged (decrement on push), `c->frame` swap, `pc=0/end=nsrc`.
  - Function exit (`fn_splice`) for `return`(0x0F)/natural body end/br-to-func:
    take = `exit_arity` (or all available when 0/harness), `code->vsn = base +
    take`, pop the `FUNC` (+ any enclosing blocks for `return`), free its frame
    (driver-owned), restore budget, advance the caller level `pc` by 1, resync
    `c->frame`. Level-0 pop → `out_done` (level-0 frame is caller-owned, never
    freed by the driver).
  - `w89_invoke` routes to `iter_invoke_driver` only when `W89_ITER=1`, the
    target is a wasm function, and `iter_invoke_safe(f)`; `iter_invoke_driver`
    seeds a heap frame (params→locals) + body range + `cfg.resn = n2`, runs the
    driver, and frees the heap frame. Everything else uses the unchanged legacy
    `A_INVOKE`/`w89_eval` path.
- `test/perf_on2.sh`: `export W89_ITER=1` (driver is still opt-in until S2.6).
- `test/run_tests.sh`: re-enabled `./test/perf_on2.sh`.
- `test/test_calls.c`: new tests `test_iter_direct_call_parity`,
  `test_iter_down_parity` (down(900) legacy vs iter), `test_iter_exhaustion`
  (unbounded recursion under iter), `test_iter_host_call` (wasm→host inline),
  plus `invoke_env` helper (requires `_POSIX_C_SOURCE` for `setenv`).
- `.agent/testing/0021` + `.agent/acceptance/0021`: added ITC-005 (eligibility
  fallback for non-migrated/indirect callees) and ITC-006 (budget bounds
  simultaneously-open frames, not cumulative work) rows + MUST-NOT lines.

## Verification performed

- `just lint ob89 test` green on the branch and on `main`.
- ob89 clean on `src/*.c` (new code hoists loads/calls/arithmetic into witness
  locals; no `&&`/`||`).
- Unit: `test_eval` + `test_calls` all pass.
- Perf gate re-enabled and passing (`down(5000)` ~15 ms).
- Conformance parity default vs `W89_ITER=1`:
  - `call.wast`: 91 passed / 0 failed on both.
  - `call_indirect.wast`: 137 passed / 0 failed on both.
  - `fac.wast`: 8 passed / 0 failed under `W89_ITER=1` (default stalls — see
    Defects/notes).

## Defects and gaps observed (handover notes for S2.4+)

1. **`skip-stack-guard-page.wast` still STALLs** (no result within 25 s) under
   both the legacy default AND `W89_ITER=1`. Not a regression from S2.3 — it is
   one of PLAN §0's known deep-recursion STALLs. This engine keeps recursion on
   a heap level stack capped by the 5000-frame budget and does NOT implement
   native-C-stack guard-page semantics, so that file cannot pass as written.
   Revisit at S2.6 (and possibly treat guard-page as an out-of-scope engine
   policy rather than a conformance requirement).
2. **Legacy default is still O(N^2) on deep recursion**, so conformance files
   with deep non-tail recursion (`fac`, `skip-stack-guard-page`) are slow/stall
   unless run under `W89_ITER=1`. This is expected until S2.6 decommissions the
   legacy path; conformance sweep runners should use `W89_ITER=1` for speed.
3. **Eligibility is intentionally conservative: any reachable body containing
   `call_indirect` is routed to the legacy path** even under `W89_ITER=1`
   (targets are unknowable statically). Consequence: the driver's own
   `call_indirect` FUNC-push is exercised only by the direct-driver unit test,
   not by conformance. Fine for S2.3; relax only if/when a runtime-fallback or
   full closure over indirect targets is wanted (S2.6).
4. **`w89_eval_iter` invoked directly (harness style) only gates the seed
   body**, not transitively-reachable callees. Driving it directly on a body
   whose callee is non-migratable would mis-run; the production `w89_invoke`
   path is safe because it applies `iter_invoke_safe` first. Keep direct-driver
   test bodies to driver-eligible callees, or fold the closure check into
   `w89_eval_iter` itself if it ever becomes a public entry again.
5. **Budget off-by-one contract**: driver allows exactly 5000 simultaneously
   open non-tail `FUNC` frames; the 5001st push reports `call stack exhausted`.
   Level-0 (the invoked function) does not consume budget. Matches `down(5000)`
   returning normally and unbounded recursion trapping; verified by the unit +
   perf tests. S2.4 tail calls must remain uncharged and re-check the same
   boundary.

## Out of scope here (next)

- S2.4 tail calls (`return_call*`, RETINV frame reuse, uncharged). Revisit
  defect note 5 (budget boundary) and note 3 (indirect).
- S2.5 exceptions unwinding on the level stack.
- S2.6 decommission of the legacy recursive path (then make driver default,
  remove `W89_ITER`, re-check skip-stack-guard-page and the full 257-file
  sweep for zero FAIL/zero STALL).
