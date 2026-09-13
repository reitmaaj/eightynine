# S2.6 iterative-machine decommission — handover (pre-decommission)

Status owner: this file. Companion to `tmp/PLAN.md` (roadmap §2.6), design
`.agent/design/0007-iterative-machine.md`, and the S2.5 handover
`tmp/S25-iter-exceptions-handover.md`. This is the *pre-decommission* handover
for S2.6, captured while the working branch `feat/s2.6-iter-decommission` is
open on top of `main`. It records the defects fixed so far, why the iterative
driver is now behaviourally clean enough to decommission, and exactly what
remains for S2.6 (legacy removal) and S2.6 completion (full-sweep gate).

The working branch is **NOT yet merged** to `main`. `main` is untouched and
green; the legacy recursive stepper remains the default; `W89_ITER=1` still
opts into the driver.

## What this handover records

Six focused changesets, each green on `just lint ob89 test`, that close the
gaps that previously blocked S2.6's "zero FAIL / zero STALL under the driver"
gate:

| Commit | Concern | Effect |
|---|---|---|
| `0923ec7` | driver control-flow: `br` to the enclosing function-body label is a return, not "undefined label"; a real function `return` keeps the top declared results (discards stray values left by an uncompleted value-typed block) | cleared ~50 `W89_ITER=1` FAILs in `func`/`return`/`br_if`/`br_table`/`block`/`if`/`unwind`/`call`/`try_table` |
| `645f1b5` | `spec_driver` reply reader | `command()` gated `readline()` on `select()` of the OS pipe; Python's `BufferedReader` read-ahead stranded the `@`-reply behind a host-print line, timing out and leaking a stale reply into the next command. Rewrote to read via `os.read` into a self-managed buffer. Unblocks files that invoke spectest `print_*` hosts (`func_ptrs`, `names`, `return_call`, `return_call_indirect`, `imports` past `:97`). |
| `80b518b` | driver `call_indirect`/`return_call_indirect` bodies | `iter_reach` rejected bodies containing indirect calls, routing them to legacy; enabling them lets deep recursion through `call_indirect` run on the flat-stack driver. `call_indirect.wast` 137/0 under the driver (was 96/0 + a STALL). |
| `65c2db0` | driver whitelist of memory loads/stores/size/grow (`0x28..0x40`) | `iter_op_migrated` omitted them, so any memory-touching body routed to legacy O(N^2) and deep recursion STALLed. They are leaf instructions run as single steps via `iter_run_plain`. `skip-stack-guard-page` 11/0 (was a ~30 s STALL), `start` 19/0. |
| `71eb253` | REPL empty-module load | `cmd_module` read 1 byte from a 0-byte file, got EOF, and returned silently with no `@`-reply, so the driver waited forever. Now reads the file's actual size and reports `@error` on a short read. `binary.wast` 125/0. |
| `bbe0baa` | spectest float-global corruption + `register` spaced names | `host.c` declared `f32v`/`f64v` as `w89_f32`/`w89_f64` but assigned bit patterns from `w89_f32_bits`/`w89_f64_bits`, so the pattern was rounded by an integer-to-float conversion (spectest `global_f32` was `0x4426a680`, not `0x4426a666`). Declared them `w89_u32`/`w89_u64`. And `repl.c` `register` only accepted a single-token name; a name with spaces (`"not wasm"`) produced no reply. Register now joins the tokens after `register` with single spaces. `imports.wast` 202/0. |

## Verification performed (branch tip, `just lint ob89 test` green)

Driver (`W89_ITER=1`) conformance is now **0 FAIL / 0 STALL** across every
previously-affected file: `func`, `return`, `br_if`, `br_table`, `block`,
`if`, `unwind`, `call`, `call_indirect`, `loop`, `skip-stack-guard-page`,
`start`, `binary`, `imports`, `func_ptrs`, `names`, `return_call`,
`return_call_indirect`, `linking`, `table`, `memory`, `load`, `store`,
`traps`, `memory_fill`, `bulk`, `try_table`, `throw`, `throw_ref`.

The driver (`w89_eval_iter`) and the legacy default now agree on all of the
above. `ob89` is clean on `src/*.c`; two `W89_ITER=1` unit parity tests added
in `test/test_calls.c` (`test_iter_br_to_func_return`,
`test_iter_return_discards_stray`) and one driver exhaustion test
(`test_iter_call_indirect_exhaustion`).

## State at handover

- Working branch `feat/s2.6-iter-decommission` off `main` (`bee7256`),
  **not merged**. Six commits as above. Legacy recursive stepper remains the
  default; `W89_ITER=1` selects the driver.
- The driver now runs: blocks/`br` (S2.2), non-tail and tail calls/frames
  (S2.3/S2.4), exceptions unwinding (S2.5), `call_indirect`/
  `return_call_indirect`, and memory leaf instructions. S2.6 (legacy
  decommission) is the remaining step.

## Remaining work (for the next session)

1. **S2.6 decommission (Phase 3).** Make the iterative driver the sole
   evaluation path and delete the legacy recursive stepper:
   - In `w89_invoke` (`src/eval.c:7132`) drop the `W89_ITER`/`iter_invoke_safe`
     branch so every wasm invocation always uses `iter_invoke_driver` →
     `w89_eval_iter`.
   - Remove the whole-body legacy fallback in `w89_eval_iter`
     (`iter_migratable(c) == 0 -> w89_eval(c)`); the driver must handle every
     reachable body.
   - Delete the legacy recursive orchestration (`step_frame`/`step_label`/
     `step_handler` C-recursion and the embedded `w89_code` `A_FRAME`/`A_LABEL`/
     `A_HANDLER` threading) while retaining the single-leaf `iter_run_plain` →
     `w89_step` execution path the driver reuses. Remove now-dead staging
     predicates (`iter_op_migrated`, `iter_instr_ok`, `iter_reach`,
     `iter_invoke_safe`) and `w89_iter_enabled`/the `W89_ITER` env read.
   - Reroute `src/instantiate.c` start/init evaluation (direct `w89_eval`
     callers, e.g. the const-expr/start paths) through the driver so module
     instantiation uses the flat-stack path.
   - Remove `W89_ITER` from tests/shell: `test/perf_on2.sh` (`export
     W89_ITER=1`), `Justfile` `spec-iter` (fold into `spec-sweep`), and the
     `setenv("W89_ITER", ...)` calls in `test/test_calls.c` /
     `test/test_exn.c`.
   - Run the full 257-file conformance sweep under the now-sole driver for
     zero FAIL/zero STALL.

2. **S2.6 gate (Phase 4).** Full 257-file sweep: zero FAIL/zero STALL with
   skips only for by-design-excluded feature modules; `just ci`
   (lint + ob89 + test + diff-sweep) green; `down(5000)` perf gate stays
   green and runaway recursion still traps at 5000. Update `tmp/PLAN.md`
   §2.6 → LANDED, design `0007` §6, and close this handover.

## Gaps / cautions for the next session

- The driver's capability whitelist (`iter_op_migrated`) is still narrower
  than the full executed instruction set: it covers control/calls/`br`/
  exceptions, `0x20..0x24` locals, `0x28..0x40` memory, and `0x41..0xC4`
  numeric/memory. `0xFC`-prefixed bulk/table/saturating ops, `0xD0..`-family
  reference ops, and `call_ref`/`return_call_ref` (`0x14`/`0x15`) still route
  bodies to legacy today. Because S2.6 removes the legacy fallback, every
  executed family must become driver-capable first — treat the leaf-safe ones
  (bulk/table, `memory.copy/fill/init`, `ref.null/is_null/func`, `drop`/
  `select`) as `iter_run_plain` single steps and add them to `iter_op_migrated`
  (mirroring the `0x28..0x40` fix), and add driver paths for the
  frame-oriented `call_ref`/`return_call_ref`.
- The `spec_driver` stdout/host-print interaction is fixed; do not revert the
  self-managed-buffer reader. Keep the spec timeout (`W89_COMMAND_TIMEOUT`)
  comfortably above the engine's worst real command (deep exhaustion is now
  fast under the driver, but module instantiation of huge bodies can still be
  slow under legacy until it is removed).
- `ob89` Obligation-C89 is strict (witness locals before branching/storing,
  no casts mixed with computation, no loads in branch conditions/call args).
  Every new/changed function must stay clean.

---

## SUPERSEDED — S2.6 decommission LANDED

This pre-decommission handover is superseded by the completed S2.6
decommission (commits on `feat/s2.6-iter-decommission`). The iterative driver
is now the sole wasm-function evaluation path:

- **Driver coverage first** (still opt-in `W89_ITER`): param-typed
  block/loop/if/try_table, table.get/set (0x25/0x26), the `0xFC`
  bulk/table/saturating family, `0xD0..0xD4` ref leaves,
  `br_on_null`/`br_on_non_null` (0xD5/0xD6), and `call_ref`/`return_call_ref`
  (0x14/0x15). A `W89_COVERAGE` diagnostic (now removed) measured the 
  driver-eligibility gap per invoked body and confirmed it reached zero.
- **Decommission**: `w89_invoke` routes every wasm function through
  `iter_invoke_driver`; `w89_eval_iter` no longer falls back to `w89_eval`;
  the `W89_ITER` env switch, the `iter_invoke_safe`/`iter_reach` eligibility
  scan, and the coverage diagnostic were removed. The bounded
  single-expression evaluator (`w89_eval`) remains for instantiation
  const/element-expression evaluation and as the unit-test reference oracle
  (flat; no control/calls/frames).
- **Verified**: full 257-file conformance 37171 passed / 0 failed / 0 STALL;
  `just ci` (lint + ob89 + test + diff-sweep 3888/0) green; `down(5000)` perf
  gate green. `.agent/tmp/s26_final_sweep.log` records the sweep.
