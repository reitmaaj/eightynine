# HANDOVER — wasm89 status

Status owner: this file. Written after the S2.7 merge and the S4 (roadmap
hygiene) docs pass. Companion to `tmp/PLAN.md` (the roadmap), the S2.7
handover `tmp/S27-constelem-driver-handover.md` (decommission LANDED), and the
v5 accepted feature corpora under `.agent/testing+acceptance/0025..0033`
(SIMD) and `0030..0033` (GC). Records the verified state, what just landed,
the agreed next milestone and its first increment, and the one open decision.

## Verified state at handover (branch `main`, tip `acead32`)

- `main` is green: `just ci` (lint + ob89 + test + diff-sweep) exit 0,
  **3888 passed / 0 failed / 0 errors**.
- Full 257-file conformance sweep: **37460 passed / 0 failed / 0 STALL**
  (skips only by-design-excluded feature modules: SIMD/GC and out-of-scope
  engine/flag gates).
- `down(5000)` perf gate green (~14-25 ms; flat native C stack).
- The iterative driver is the sole wasm evaluation path. The legacy
  C-recursive evaluator `w89_eval` is fully deleted; instantiation
  const/element expressions and host functions run on/off the driver.

## What just landed (most recent)

- **S3.1 B1-b (v128 first-class + `v128.const`/`load`/`store`) merged to `main`**
  as `acead32`, this session. Re-enabled `0xFD` decode/validate/eval for
  `v128.const` (0x0C), `v128.load` (0x00), `v128.store` (0x0B), made v128 a
  first-class value type (params/locals/globals) with global const-expr
  `v128.const` running on the iterative driver, and admitted `v128.const` in
  `check_const`/`is_const_instr`. `simd_linking.wast` 3/0/0 and
  `simd_const.wast` 373/0/385 are now real module-validation assertions (their
  `assert_return` value commands still skip pending driver v128 marshalling,
  B1-c). Full sweep 37460/0/0; differential 3888/0. Docs: design 0010 §2,
  PLAN §0/§6, `tmp/S31-simd-b1-status.md`. Branch `feat/s3.1-simd-b1b` deleted
  after merge.
- **S3.1 B1 (128-bit runtime value) merged to `main`** as `fee767e`, this
  session. Single direct widening of the shared value model: `w89_value`
  (`eval.h`) gains a `w89_v128` (16-byte) union member via the new `module.h`
  type; every value is now one full 16-byte stack element so all splice/save
  sites full-copy and a vector can never alias/truncate a scalar;
  `w89_value_v128` constructor; scalar `u.num` fast path unchanged.
  Working branch `feat/s3.1-simd-b1` deleted after merge.
- **S2.6 + S2.7 merged to `main`** (13 commits, fast-forward from `bee7256` to
  `a9e2390`): S2.6 iterative-machine decommission, then S2.7 full legacy
  deletion (const/elem `ref.func` slice bug fixed; `w89_eval` deleted; host
  invocation as leaves; oracle parity tests reworked to fixed expectations).
  See `tmp/S27-constelem-driver-handover.md`.
- **S4 roadmap-hygiene docs pass merged as `03e7cea`**: refreshed
  `tmp/PLAN.md` §0 snapshot to 37171/0/0; marked the S2.1–S2.5 `W89_ITER`
  opt-in narrative as historical via a §2 reading note; recorded
  `wip/wid-host-module` as open/undecided in §5; updated the §6 checklist.
  Docs-only; working branch `docs/s4-plan-hygiene` deleted after merge.

Working branches from all the above were deleted after merge. `main` is clean.
Not pushed upstream.

## Next milestone (Phase B, in progress)

The milestone is **S3.1 SIMD fixed-width v128**, then **S3.2 GC + deep
function-references**. Order chosen by the user: **S4 hygiene (done) → S3.1
SIMD → S3.2 GC.**

Phase B plan (branch off `main`, TDD per increment, gate `just ci` + the
relevant `simd_*.wast` subset → full sweep → differential; design
`.agent/design/0010-simd.md`, corpora `.agent/testing+acceptance/0025..0028`):

1. **128-bit runtime value** (design `0010` §2): **LANDED** as `fee767e`
   (single direct widening).
2. **Decoder `0xFD` + validator + evaluator `step_simd`** for
   `v128.const`/`load`/`store` + v128 as a first-class value type incl. global
   const-exprs: **LANDED** as `acead32` (kept the sweep green by running
   `simd_const`/`simd_linking` module commands as real assertions).
   See `tmp/S31-simd-b1-status.md`.
3. **B1-c — driver-side v128 value marshalling**: make `test/spec_driver.py
   value_token()` serialize a JSON v128 (`{"type":"v128","lane_type",...}`)
   into a v128 byte token for all lane types (i8/i16/i32/i64/f32/f64 incl.
   float/nan literals byte-matching wat2wasm) and teach `repl.c
   fmt_value`/arg-parsing to print/accept a 16-byte v128 token. Un-skips the
   ~385 `simd_const` `assert_return` value commands. Independent of the C
   engine (byte-exact via the module binary).
4. **Lane kernel library** (deterministic float NaN profile shared with
   scalar), then **conformance** `simd_*.wast` subset → full, then
   **differential** (SIMD engine flag + NaN rule).
Then relaxed SIMD as the S3.1 tail increment.

Phase C (S3.2 GC + deep funcref) is downstream; design
`.agent/design/0011-gc.md`, corpora `0030..0033`. It additionally needs the
upstream `gc/` testsuite vendored into `vendor/testsuite-main` before it can
gate.

## Open decisions

- **B1-c scope**: driver-side v128 value marshalling (spec_driver serialize +
  repl fmt/parse) — the natural next increment; kernels (B2) may follow
  separately. Not started.
- **`wip/wid-host-module`**: open / undecided. Branch kept parked as-is
  (commit `a470943`, ~83-line `w89_host_module` WID stub, ~60+ commits behind
  `main`). Not merged, not deleted. Recorded in `tmp/PLAN.md` §5.
- **Upstream `main`**: never pushed without explicit permission.

## Branch / git state

- Current: `main` at `acead32`, clean (only untracked `.agent/tmp/`).
- Branches present: `main`, `feat/a3-on2-iterative`, `wip/wid-host-module`.
