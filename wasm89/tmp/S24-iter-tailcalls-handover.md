# S2.4 iterative tail calls — implementation handover

Status owner: this file. Companion to `tmp/PLAN.md` (roadmap §2.4) and
`tmp/S23-iter-calls-handover.md`. This is the *post-implementation* handover
for S2.4, which is landed on a working branch ready to merge to `main`. It
records state, what changed, verification, and every defect/gap observed for
S2.5 (exceptions unwinding) and S2.6 (decommission).

## Definition of done met

Acceptance `.agent/acceptance/0022-iter-tailcalls.md` (ITT-001/002) and
scenarios `.agent/testing/0022-iter-tailcalls.md` are covered and green via
the new driver-forcing unit tests in `test/test_calls.c`. Tail transitions are
in-place and uncharged; a 1M-iteration self `return_call` loop completes
without exhausting the budget. Default path stays legacy and byte-identical.

## State at handover

- Working branch `feat/s2.4-iter-tailcalls` off `main` (`bb76c65`), not yet
  merged. Legacy recursive stepper remains the default; `W89_ITER=1` selects
  the driver.
- Driver (`w89_eval_iter`, `src/eval.c`) now migrates `return_call`(0x12) /
  `return_call_indirect`(0x13) in addition to the S2.2 block/`br` and S2.3
  `call`/`call_indirect`/`return` migration. S2.5 exceptions unwinding and S2.6
  decommission: NOT done.

## What changed (files)

- `src/eval.c`:
  - `iter_op_migrated` now also returns migrated for `0x12`(return_call) and
    `0x13`(return_call_indirect). `0x14`/`0x15` (call_ref/return_call_ref)
    remain unmigrated (S3.2 funcref).
  - `iter_instr_ok` now also rejects `0x13` when `allow_indirect == 0`, so
    reachable bodies containing an indirect tail call still route to the legacy
    path under `W89_ITER=1` (conservative: targets unknowable statically),
    mirroring the S2.3 `call_indirect` policy (handover note #3).
  - The `call`(0x10)/`return_call`(0x12) direct resolution and the
    `call_indirect`(0x11)/`return_call_indirect`(0x13) indirect resolution are
    each factored behind a shared label (`resolve_direct`, `resolve_ind`), with
    an `istail` flag deciding `do_call` vs `do_tail` after the callee is
    resolved. No duplication; `call`/`call_indirect` behaviour unchanged.
  - New `do_tail:` path in the driving loop:
    - Computes the enclosing function level (`cfi = iter_func_of`), discards
      any enclosing `BLOCK` levels (`c->ln = cfi+1`) since a tail call never
      resumes them.
    - Host callee: inlines the host call at `cbase`, then marks the enclosing
      `FUNC` terminal (`pc = nsrc`) and `continue`s so the existing `fn_splice`
      returns the host results to the function's caller (uniform, no frame).
    - Wasm callee, `cfi > 0` (driver-owned frame): frees the frame and
      reallocates the callee frame (`n1c + nlocals_callee`), copies
      args→locals, initialises declared locals, `code->vsn = cbase`, and
      overwrites `lvls[cfi]` in place (src/nsrc/pc=0/end/base/exit_arity=n2c).
      **No budget charge, no level-stack growth.**
    - Wasm callee, `cfi == 0` (the seed; its frame is caller-owned and never
      freed by the driver): if the callee fits in the existing seed frame it is
      reused in place (uncharged); otherwise it falls back to a single charged
      `do_call` push with the seed marked terminal so the seed returns after
      the callee (bounded — one charge never exhausts a tail loop).
- `test/test_calls.c`: new driver-forcing tests
  `test_iter_tail_direct` (direct-driver self `return_call` countdown),
  `test_iter_tail_indirect` (direct-driver self `return_call_indirect`
  countdown), plus `test_iter_tail_self_loop` (1M self tail under `W89_ITER=1`),
  `test_iter_tail_self_parity`, `test_iter_tail_seed_oversized` (seed frame too
  small for a larger-locals tail callee), and `test_iter_tail_host`
  (wasm→host tail). No `.agent/testing`/`.agent/acceptance` edit was needed:
  `0022` already encoded ITT-001/ITT-002.

## Verification performed

- `just lint ob89 test` green on the branch; ob89 clean on `src/*.c`.
- Unit: `test_eval`, `test_calls` all pass; `down(5000)` perf gate still green
  (~7 ms).
- Conformance under `W89_ITER=1` (driver): `call.wast` 91/0, `call_indirect.wast`
  137/0, `fac.wast` 8/0 — identical to the S2.3 handover, confirming the shared
  `resolve_direct`/`resolve_ind` refactor did not regress non-tail calls.
- `return_call.wast` under `W89_ITER=1`: 32 passed / 1 failed. The single
  failure (`tailprint_i32_f32`, line 140) is the known **stdout-print harness
  artifact**: the spec driver reads the runtime's stdout for the `@return`
  terminator, so any spectest host that *prints to stdout* (e.g. `print32`,
  `print_i32_f32`) reads as "STALL — no reply". The baseline sweep stalls on
  plain non-tail `print32` in `imports.wast`/`names.wast` identically; it is not
  a tail-call or S2.4 regression.

## Defects and gaps observed (handover notes for S2.5+)

1. **Seed (`cfi==0`) oversized tail callee uses a charged fallback**, not a
   true in-place reuse, because the seed frame is caller-owned and must not be
   freed or resized by the driver. Bounded (one charge), never exhausts a tail
   loop; revisit if strict "uncharged at the seed" semantics are ever wanted
   (would require the driver to own the level-0 frame — incompatible with the
   direct-driver unit tests that pass a stack frame).
2. **`return_call` to a host that writes to stdout cannot be conformance-verified**
   end-to-end under this spec harness (see above); the driver path is covered by
   `test_iter_tail_host`. Not an engine defect.
3. **`return_call_indirect` under the driver is exercised only by the direct
   unit test** (bodies with indirect tails are conservatively routed to legacy),
   mirroring S2.3 note #3 for `call_indirect`. Relax only if/when a
   runtime-fallback or full closure over indirect targets is added (S2.6).
4. **`return_call_ref`/`call_ref` (0x15/0x14) are still unmigrated** and route
   to the legacy path; they belong to the S3.2 typed-function-references stream,
   not S2.4.
5. **Budget contract unchanged**: only non-tail `FUNC` pushes decrement the
   5000 budget; tail transitions never do. S2.5 exceptions must unwind the level
   stack without leaving charged or stale `FUNC` levels behind.

## Out of scope here (next)

- S2.5 exceptions unwinding on the level stack (revisit note 5; `return_call`
  inside a `try_table` arm must still tail-reuse correctly).
- S2.6 decommission of the legacy recursive path (make the driver default,
  remove `W89_ITER`, re-check skip-stack-guard-page, the stdout-print harness
  artifact, and the full 257-file sweep for zero FAIL/zero STALL).
