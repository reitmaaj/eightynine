# wasm89 — Roadmap to full WebAssembly Core 3.0

Status owner: this file. Purpose: a single working spec + plan covering every
remaining step until wasm89 satisfies the whole WebAssembly Core Specification
**release 3.0** (the concept's definition of done), including the iterative
evaluator migration and the differential harness.

> By-design exclusions (concept `.agent/concept/0000-concept.md`): text-format
> parsing (ch.6, tooling only), WASI and all non-core host APIs, threads /
> shared memory (atomics), and the component model. These are NOT tracked here
> as work; they are intentionally absent.

---

## 0. Current state (snapshot at plan time)

- `main` is green: `just ci` = lint + ob89 + test + `diff-sweep` (3888 cases /
  0 mismatches). Full 257-file conformance: **37460 passed / 0 failed / 0 STALL**
  (skips are only by-design-excluded feature modules: SIMD/GC and, where
  applicable, out-of-scope engine/flag gates).
- Decoder/validator/evaluator present for: scalar numerics + conversions +
  saturating truncation, control flow, calls (+`call_indirect`), memory + bulk +
  `memory.fill/copy/init`, tables + `call_indirect`, globals (const), references
  (null/func/extern/exn), multi-value, **exceptions** (`tag`/`throw`/
  `throw_ref`/`try_table`), **multi-memory**, **memory64**.
- Differential harness (`test/diff/`) green vs wasmtime + wasm-interp, incl.
  exceptions (uncaught + `try_table` catch-and-return) and memory64/multi-memory
  templates.
- S2.1 scaffold merged (`w89_lvl` level stack + unified value stack +
  `w89_eval_iter`).
- Iterative machine landed through **S2.6** and completed in **S2.7**:
  blocks/`br` (S2.2), non-tail calls/frames (S2.3), tail calls (S2.4),
  exceptions unwinding (S2.5), the S2.6 decommission that made the iterative
  driver the sole wasm-function evaluation path (`W89_ITER` removed), and
  the S2.7 full deletion of the legacy C-recursive evaluator `w89_eval`
  (instantiation const/element expressions and host functions now also run
  on/off the driver). There is exactly one wasm evaluation path.
- S3.1 SIMD partially landed: the **128-bit runtime value** (B1) and the
  `v128.const`/`load`/`store` decode→validate→eval with v128 as a first-class
  value type incl. global const-expr (B1-b). Driver-side v128 value
  marshalling (B1-c) and the lane kernels + real-file conformance enablement
  (B2) are still to come; `v128.const` no longer decode-rejects, so
  `simd_linking.wast` + `simd_const.wast` module commands are real assertions
  (their `assert_return` value commands still skip pending B1-c). **GC + deep
  function-references** (S3.2) remains NOT implemented; the decode rejects GC.
  Acceptance-first corpora for the SIMD/GC streams exist
  (`.agent/testing+acceptance/0025..0033`).

Definition of done for the whole roadmap: the 257-file conformance sweep has
**zero FAIL and zero STALL**, with no SIMD/GC skip except modules exercising
by-design-excluded features; `just ci` green; and every non-excluded instruction
is also exercised by the differential harness.

---

## 1. Working rules (applied to every step)

- Short-lived branch off `main`; every change gated green before merge:
  `just lint`, `just ob89`, `just test`, and where behaviour is affected the
  relevant conformance subset, then the full sweep, then `diff-sweep`.
- `main` must stay green at all times. Red-only-on-branch gates (e.g. the perf
  gate) are re-wired only when they turn green.
- TDD: scenario (`.agent/testing/NNNN-*.md`) → acceptance
  (`.agent/acceptance/NNNN-*.md`) incl. unacceptable-behaviour → failing test →
  minimum code → green → refactor. Concept/design/stories docs kept current.
- C89-clean: `-std=c89 -pedantic-errors -Wall -Wextra -Werror`, four-space
  indent, small pure functions, `ob89` obligations model clean.
- Differential features are enabled only after the interpreter runs them
  (decode→validate→eval green) so a differential mismatch is a real finding.

---

## 2. S2 — Iterative machine (perf/correctness of the evaluator core)

Rationale/design: `.agent/design/0007-iterative-machine.md`; the O(N²)
re-descend comes from `step_frame`/`step_label`/`step_handler` C-recursion
(`eval.c` ~1728/2964/1275). Goal: an explicit heap `w89_lvl` machine stack +
one unified `w89_value vs[]` so each step is O(1) amortized and native stack
stays flat.

State: S2.1 scaffold merged (containers + `w89_eval_iter` stub, legacy default).

> **Reading note.** S2.1–S2.5 below document the staged, side-by-side
> migration as it was planned and executed (`W89_ITER=1` opt-in, legacy path
> still the default through S2.5). That is now **historical**: S2.6 made the
> iterative driver the sole wasm-function evaluation path and removed the
> `W89_ITER` switch, and S2.7 deleted the legacy C-recursive evaluator
> `w89_eval` entirely (const/elem and host invocation also run on/off the
> driver). The subsections are kept for design history; the live status is in
> §0 and §2.6/§2.7.

### 2.1 Spec — target machine model

- One `w89_lvl` per open function frame (`W89_LVL_FUNC`) or structured block
  (`W89_LVL_BLOCK`): `src/nsrc` instr range, `pc`, `end`, loop `contpos/contn`,
  `exit_arity`, value-stack `base`, and (blocks) `catches` arms.
- One config-level value stack `vs[]/vsn`; each level records `base = vsn` at
  entry; a level yields `exit_arity` results above `base` on exit.
- Exhaustion budget = max simultaneously-open non-tail `FUNC` levels (5000);
  tail calls and blocks are uncharged.
- Driving loop (`w89_eval_iter`), stepping the top level in place:
  1. `FUNC` at `return` → pop, splice results into caller (O(1)); repeat while
     the new top is at a return.
  2. else dispatch `src[pc]`:
     - plain/`const`/numeric/memory/etc. → execute and advance `pc`;
     - `block`/`loop`/`if` → push `BLOCK` (if: eval cond, choose arm; else picks);
     - `call`/`call_indirect` → push `FUNC` (host calls inline);
     - `return_call*` → replace top `FUNC` reusing the value frame;
     - `br`/`br_if`/`br_table` → pop levels to the target label, honor arity /
       loop re-entry; `br` on a `FUNC` = return;
     - `try_table` → push a `BLOCK`-like level carrying catches;
     - `throw`/`throw_ref` → unwind to nearest matching catch or surface as an
       exception result.
- Migration is staged side-by-side: new driver handles migrated kinds and falls
  back to the legacy path for not-yet-migrated kinds; `W89_ITER=1` selects the
  driver, default remains legacy until a feature migrates, then legacy is
  deleted (S2.6).

### 2.2 Spec + plan — S2.2 migrate blocks/`br`/`if`/`loop`/`br_table` → `BLOCK`

Scope: make the iterative driver execute function bodies whose instructions are
already "migrated" = plain/`const`/numeric/compare/convert/local/global/
`drop`/`select`/`unreachable` plus the structured block instructions below. Any
not-yet-migrated instruction (call, memory/table, exceptions, tail) falls back
to the legacy path.

Steps (each ends green on branch; gate = `just lint ob89 test` + the
`br`/`block`/`if`/`loop`/`br_table` conformance subset; legacy default
behaviour unchanged):
1. **Top-of-stack dispatcher.** Implement `w89_eval_iter` loop that walks the top
   `BLOCK`/`FUNC` level: advance `pc`, dispatch migrated opcodes directly, and
   return a "needs-legacy" signal for unmigrated kinds so the caller re-enters
   the legacy `w89_eval` for that function. Unit-test via a new `W89_ITER=1`
   path that reproduces existing `test_eval` block/if/loop/br/br_table results.
2. **`block`/`loop`/`if` push & pop.** Emit `BLOCK` levels with correct
   `end`/`exit_arity`/`contpos`; on normal completion pop and splice results;
   verify multi-value block results.
3. **`br`/`br_if`/`br_table` pop-to-label.** Pop down to target level, splice
   its results; loop `contpos/contn` re-entry; `br_table` bounds = default.
4. Wire the conformance driver to run under `W89_ITER=1` and require identical
   results to the legacy path for the block conformance files (differential of
   the two internal paths).

Exit: block control-flow conformance green under the iterative driver; legacy
still default and bit-identical.

### 2.3 S2.3 — migrate calls/frames (non-tail)
Push/`FUNC` pop for `call`/`call_indirect`; locals/params into `w89_frame`.
**Perf gate `test/perf_on2.sh` flips green here** (`down(5000)` O(N), flat C
stack). Verify `call`, `call_indirect`, `fac`, `skip-stack-guard-page`.
Then re-enable `perf_on2.sh` in `test/run_tests.sh` and merge to `main`.

### 2.4 S2.4 — migrate tail calls (`return_call*`)
Frame reuse, uncharged. Verify `return_call*` (1M loops succeed).
**Status: LANDED on `main`** (at the time: opt-in via `W89_ITER=1`; legacy default
unchanged). `return_call`(0x12)/`return_call_indirect`(0x13) reuse the driver's
`FUNC` level in place, uncharged; seed/caller-owned-frame tails reuse in place
when the callee fits or fall back to a single bounded charged push otherwise;
wasm→host tails are inlined and return through `fn_splice`. `return_call_ref`
(0x15) stays on the legacy path (S3.2 funcref). See
`tmp/S24-iter-tailcalls-handover.md`.

### 2.5 S2.5 — migrate exceptions unwinding
`try_table` level + `A_THROWING`-equivalent unwind to nearest catch or surface
`W89_EVAL_EXCEPTION`. Verify `throw`, `throw_ref`, `try_table`.
**Status: LANDED** (at the time: opt-in via `W89_ITER=1`; legacy default unchanged).
`try_table`(0x1F) becomes a `BLOCK` level carrying its catch arms;
`throw`(0x08)/`throw_ref`(0x0A) unwind open levels to the nearest matching
catch (freeing driver-owned frames and restoring the budget) or surface as
`W89_EVAL_EXCEPTION`. Exceptions conformance (`throw`/`throw_ref`/
`try_table`) is green and byte-identical under `W89_ITER=1` and the legacy
default. See `tmp/S25-iter-exceptions-handover.md`.

### 2.6 S2.6 — decommission legacy
Remove the legacy recursive stepper and the `c->code`-recursion path; make the
iterative driver the sole path. Full 257-file sweep must show **zero FAIL and
zero STALL** (the 10 current STALLs must be gone). `W89_ITER` env removed.
**Status: LANDED.** The iterative driver is the sole path for every wasm
function invocation (`w89_invoke`, start via `w89_invoke`); the `W89_ITER`
env switch, the driver-eligibility scan (`iter_invoke_safe`/`iter_reach`),
the `w89_eval_iter` legacy fallback, and the coverage diagnostic are removed.
The driver now runs param-typed blocks, table.get/set, the `0xFC` bulk/table/
saturating family, the `0xD0..0xD4` reference leaves,
`br_on_null`/`br_on_non_null`, and `call_ref`/`return_call_ref`. Full
257-file conformance: 37171 passed / 0 failed / 0 STALL; `just ci`
(lint+ob89+test+diff-sweep) green; `down(5000)` perf gate green.

### 2.7 S2.7 — full legacy deletion (const/elem + host on the driver)
Close the S2.6 loose end: run instantiation const/element expressions and
host invocation on the iterative driver and delete the legacy recursive
evaluator entirely.
**Status: LANDED.** `eval_const`/`eval_elem_exprs` hand the driver the exact
expression slice (`code->src`/`nsrc`); fixing a bug where every element slot
re-ran slot 0 (a later `ref.func` slot in elem.wast:175 came back null).
Host functions are invoked directly as leaves (`invoke_host_direct`). The
C-recursive `w89_eval` and its declaration are deleted; no source path or
unit-test oracle references it (oracle parity tests now assert concrete
expected results). Full 257-file conformance: 37171 passed / 0 failed /
0 STALL; `just ci` green.

---

## 3. S3 — New spec feature streams (decode → validate → eval → conformance → differential)

General sub-step template per feature batch:
concept/design → stories → testing/acceptance (incl. unacceptable cases) →
decode (+ type tables) → validate → eval → smoke + unit → conformance subset →
full sweep → differential enable (engine parity flags + NaN/determinism rules).

### 3.1 SIMD (fixed-width v128, then relaxed SIMD)

1. **v128 value type + decoder**: `v128` type, lane immediates, `i8x16/…`,
   `v128.const`, `v128.load/store` with lane shapes; new `w89_v128` value.
2. **Validator**: lane-typed operand stack rules for every SIMD instruction.
3. **Evaluator**: numeric kernels for integer/float lane ops, shuffles,
   splats, extract/replace, comparisons, bitmask, load/store lanes, dot, extmul.
4. **Conformance**: SIMD testsuite subset → full.
5. **Relaxed SIMD**: deterministic-profile behaviours (fixed result choice).
6. **Differential**: v128 exact-bits compare vs wasmtime + wasm-interp; define a
   SIMD NaN/determinism rule consistent with the deterministic profile.

### 3.2 Function references (deep) + GC

1. **Function references**: typed `ref.func`/`call_ref`/`ref.as_non_null`/
   `br_on_null`/`ref.is_null`/`ref.null` for typed func types; recursion groups
   already exist in types. (Prerequisite for GC.)
2. **GC core**: struct/array types, `struct.new/get/set`, `array.new/get/set/
   len`, `i31.new/get_s/get_u`, `ref.eq`, `ref.test`/`ref.cast` (+null
   variants), `anyref`/`eqref`/`structref`/`arrayref`/`i31ref` type hierarchy,
   subtyping already present.
3. **Conformance**: gc.wast family → full.
4. **Differential**: GC refs (identity/equality semantics), plus engine flags.

Each of 3.1/3.2 is many independently-mergeable increments; milestones tracked
in `.agent/design` as they land.

---

## 4. S1 — Differential harness (remaining, optional)

- **wasmer** as a 4th oracle (needs install/build go/no-go).
- Fuzz breadth: deeper expressions, more seeds, larger default sweep once perf
  (S2) makes runs cheap.
- Anything from S2/S3 becomes differential-enabled as it lands.

---

## 5. S4 — Verification & hygiene

- Re-run and keep green the full 257-file conformance sweep; record results per
  change in `.agent/tmp/`.
- `just ci` = lint + ob89 + test + `diff-sweep` (done).
- Fate of `wip/wid-host-module`: **open / undecided.** Branch kept parked as-is
  (single commit `a470943` adding an ~83-line `w89_host_module` stub for WID
  imports, now ~60+ commits behind `main`). Not merged, not deleted; revisit if
  a WID host-module integration is ever wanted. It is a separate, experimental
  concern and does not affect the core wasm roadmap.
- Keep `.agent/concept|design|stories|testing|acceptance` current; update this
  PLAN.md as milestones land.

---

## 6. Feature→conformance checklist (definition of done)

For each feature below, done = full-sweep green (no FAIL/STALL), `just ci`
green, and (where applicable) differential-covered:

- [ ] Scalar numerics + conversions + saturating truncation — DONE
- [ ] Control flow / multi-value — DONE (S2.2 iterative; sole path since S2.6)
- [ ] Calls + call_indirect — DONE (S2.3 iterative; sole path since S2.6)
- [ ] Tail calls (`return_call*`) — DONE (S2.4 iterative; sole path since S2.6)
- [ ] Memory/bulk/table/refs/globals — DONE
- [ ] Exceptions (tag/throw/throw_ref/try_table) — DONE; S2.5 iterative DONE
- [ ] memory64, multi-memory — DONE; differential templates DONE
- [ ] Iterative machine S2.1–S2.7 — DONE (sole path; legacy evaluator fully
  deleted; see §2.6/§2.7)
  (acceptance-first corpus DONE: `.agent/testing+acceptance/0020..0024`)
- [ ] SIMD v128 (fixed) — PENDING (S3.1). B1 (128-bit runtime value widening)
  LANDED; B1-b (`v128.const`/`load`/`store` decode→validate→eval + v128 as a
  first-class value type + global const-expr) LANDED, making
  `simd_linking.wast` + `simd_const.wast` module-validation commands real
  assertions (full sweep 37460/0). Remaining: B1-c driver v128 value
  marshalling (to un-skip simd_const's `assert_return` value commands), then
  B2 lane kernels + real-file conformance subset→full (see
  `tmp/S31-simd-b1-status.md`, design 0010 §2)
  (acceptance-first corpus DONE: `.agent/testing+acceptance/0025..0027`)
- [ ] Relaxed SIMD — PENDING (S3.1)
  (acceptance-first corpus DONE: `.agent/testing+acceptance/0028`)
- [ ] Function references (deep) — PENDING (S3.2 prereq)
  (acceptance-first corpus DONE: `.agent/testing+acceptance/0030`)
- [ ] GC (struct/array/i31/ref.eq/casts) — PENDING (S3.2)
  (acceptance-first corpus DONE: `.agent/testing+acceptance/0031..0033`)
- [ ] Full 257-file sweep zero FAIL/zero STALL — PENDING (S2 complete at
  37171/0; now blocked only on the S3 SIMD/GC feature streams)
- [ ] Differential zero mismatches across all the above — PENDING

Tracked exclusions (by design, not work): text format (ch.6), WASI, threads /
atomics, component model.
