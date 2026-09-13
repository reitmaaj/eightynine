# S2.3 iterative calls/frames — implementation handoff

Status owner: this file. Companion to `tmp/PLAN.md` (roadmap) and
`.agent/design/0007-iterative-machine.md`. Intended for a fresh session to
land S2.3 (PLAN §2.3): migrate non-tail `call`/`call_indirect`/`return`/
frames onto `FUNC` levels so the iterative driver runs real function
invocations on the level stack, flipping `test/perf_on2.sh` green
(`down(5000)` O(N), flat C stack).

Definition-of-done acceptance: `.agent/acceptance/0021-iter-calls.md`
(ITC-001..004); scenarios `.agent/testing/0021-iter-calls.md`. Cross-ref
the whole remaining S2 sequence in `PLAN.md` §2 and `.agent/design/0007`
§2.3/§3/§4 (tail S2.4, exceptions S2.5, decommission S2.6).

## State at handoff

- `main` @ `076d27c`: S2.2 landed and green (acceptance corpus, parity
  guard, iterative `BLOCK` driver `w89_eval_iter`, 8000-deep-nesting test).
  Legacy recursive stepper is the default; conformance unchanged.
- `w89_eval_iter` (`src/eval.c:5014`) + helpers `iter_op_migrated`,
  `iter_migratable`, `iter_run_plain`, `iter_br`. Driver is dormant in
  production (only test-invoked) — zero behavioral risk on `main`.
- The driver currently handles `block`/`loop`/`if`/`br`/`br_if`/`br_table`
  + plain scalar/local/global ops over a **harness-style** body (level-0
  `FUNC`, results = leftover values, no real frame). It does NOT yet handle
  calls or real function frames.
- S2.3 work branch: `feat/s2.3-iter-calls` (empty, off `main`).

## Legacy semantics to replicate (fully extracted, file:line)

Entry: `w89_invoke` (`eval.c:6038`) builds an empty `cfg.frame`, pushes args
onto `cfg.code.vs`, pushes `A_INVOKE(finst)`, calls `w89_eval`.

`step_invoke` (`:1561`):
- Budget gate: `if (c->budget == 0) → W89_STEP_EXHAUSTED` (`:1601-1605`).
- Host: run `f->host` inline on args at `code->vs[base]`, set `vsn = base`,
  append `nres` results (`:1614-1634`); no frame left open.
- Wasm: `n1`/`n2` = func params/results via `w89_func_arity`; require
  `vsn >= n1`; `base = vsn - n1`. `frame_alloc(inst, nlocals=n1+nlf)`
  (`:1482`), copy top `n1` args → `locals[0..n1)` (`.set=1`), default-init
  the `nlf` declared locals (`:1651-1671`; non-nullable ref decls stay
  `.set=0`). `code->vsn = base` (pop the args). Build `A_FRAME(n=n2, frame)`
  whose embedded code is `A_LABEL(n=n2, src=fn->code.items, nsrc=ni)` over
  the whole body. Caller values below `base` remain on the caller stack.

`step_frame` (`:1706`):
- Body exhaustion (`code_head==0`, `:1737`): append whole body `vs` to the
  caller, free frame.
- `A_RETURNING` (`:1757`): append `take_n = min(nvs0, fr->n)` results, free
  frame.
- `A_RETINV` (`:1788`): tail call → in-place `A_INVOKE` switch (S2.4, NOT
  here).
- any `is_jumping` terminal (`:1822`): propagate outward, free frame.
- else: step the body one step with `budget-1` (`:1833-1849`) — this is the
  C-recursion S2.3 must eliminate.

`step_return` captures the whole current `code.vs` as `A_RETURNING.vs0`;
`step_frame` splices `take_n = min(nvs0, n2)` results.
`step_call` (`:1852`) resolves `inst->funcs[idx]` → pushes `A_INVOKE`
(hosts run inline via `step_invoke`). `step_call_indirect` (`:1952`)
resolves a table element to a funcinst then behaves as a call (bounds +
type check identical to legacy).

Budget: `w89_i64`, one unit per open non-tail frame (each nested frame body
runs at `budget-1`); tail/blocks uncharged. Exhaust at 0. `c->exhausted`
flag set. `down(5000)` terminates (5000 open non-tail frames are allowed);
runaway traps at 5000 fast.

## Driver-side migration (do NOT alter legacy; it stays default & exact)

Extend the S2.2 driver. Two gaps:

1. **Calls/return in the driver.**
   Add `0x10`(call), `0x11`(call_indirect), `0x0F`(return) to
   `iter_op_migrated`. In the main loop:
   - `call`/`call_indirect`: resolve funcinst. Host → run inline against
     `c->code.vs` (args on top; mirror `step_invoke` host branch). Wasm →
     allocate frame (`frame_alloc`), copy top `n1` args → locals, init the
     `nlf` declared locals, `c->code.vsn = vsn - n1`, then push a `FUNC`
     level (`lv.kind = W89_LVL_FUNC`, `frame`, `src=fn->code.items`,
     `nsrc=fn->code.n`, `pc=0`, `end=n`, `exit_arity=n2`, `base=vsn-n1`,
     `finst=f`). Decrement `c->budget`; if it would go negative / reaches 0
     → exhausted. Caller values below the call base stay on `c->code.vs`.
   - `return` and FUNC body exhaustion: pop the `FUNC` level, splice
     results: `take_n = min(results above FUNC.base, n2)` into the caller's
     operand region (the parent level's frame), resume parent at
     `pc_call + 1`, `frame_free`. Mirror `step_frame`/`step_return`.
   - Keep the S2.2 harness path (level-0 FUNC, results = leftover) intact
     for the test harness; real driver-handled `call` targets enforce
     `exit_arity`.
2. **`w89_invoke` under `W89_ITER=1`.** Route it through the driver only
   when `w89_iter_enabled()`: seed the driver from `f` directly (frame with
   params→locals + body range) instead of `A_INVOKE`. Default (no env) must
   keep calling `w89_eval` — byte-identical. This is what makes real
   block+call conformance runnable under `W89_ITER=1` (0021 / 0020
   ITB-004).

## Correctness hotspots (mirror legacy exactly)
- `down(5000)` O(N): driver pushes callee `FUNC` levels and steps the top in
  place — never C-recurse into `step_frame`/`step_invoke`/`step_label`.
- Budget charge: only non-tail `FUNC` pushes; no false exhaustion below
  5000; runaway traps at 5000.
- Result splice `take_n = min(nvs0, n2)`; multi-value order preserved;
  caller values below the call base untouched.
- Host calls inline; no `FUNC` level left open.
- `call_indirect` bounds/type check identical to legacy.
- Frames freed exactly once on every exit path.
- ob89 obligations apply to `src/*.c`: simple store RHS, hoisted loads and
  indices (`li = ln - 1`), no `&&`/`||`, `sizeof(type)`, no calls/loads in
  branch conditions.

## Test sequence
1. Parity guard for migrated programs now including calls; existing
   `test_calls.c` and block/loop/br tests stay green on legacy default.
2. A real recursive module driven through the driver: `down(N)` non-tail
   depth 5000 fast + correct; runaway traps at budget. Verify `fac`,
   `skip-stack-guard-page`.
3. Re-enable `test/perf_on2.sh` in `test/run_tests.sh` (acceptance 0021 /
   0014) and merge to `main` only once it passes.
4. `just lint ob89 test` green; ob89 clean; default legacy byte-identical.

## Out of scope here
- S2.4 tail calls (`return_call*`, RETINV frame reuse, uncharged).
- S2.5 exceptions unwinding on the level stack.
- S2.6 decommission of the legacy recursive path.
