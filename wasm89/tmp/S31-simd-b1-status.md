# S3.1 SIMD — B1 (128-bit runtime value) status

Status owner: this file. Companion to `../PLAN.md` §3.1 and
`.agent/design/0010-simd.md`. Records the S3.1 SIMD increments B1 (runtime
value widening) and B1-b (v128.const/load/store first-class), and the
verified gates.

> B1-b LANDED on `main` (branch `feat/s3.1-simd-b1b`): the earlier "deferred"
> framing below is superseded — see the "Status: B1-b LANDED" section at the
> end for the current state and the B1-c next step.

## What B1 delivers (the roadmap's first, highest-risk increment)

The core `w89_value` 128-bit widening, as a **single direct change**:

- `module.h`: new `w89_v128` type (16-byte raw byte carrier; lane `i` at byte
  `i`, matching little-endian wasm memory).
- `eval.h`: `w89_value` union gains a `vec` (`w89_v128`) member; every value
  is now one full 16-byte element on the unified value stack, so all
  splice/save sites (`c->code.vs`, config `vs`, `w89_local`,
  `w89_globalinst.value`) copy the whole struct — a vector can never alias or
  truncate a neighbouring scalar, and scalars keep the `u.num` fast path.
- `eval.c`: constructor `w89_value_v128`.

Rationale for keeping it isolated: this touches the shared scalar layout used
by every numeric/memory/control/ref path, so the scalar suite is the guard.
A v128 MUST stay a distinct (non-ref) operand type; validation rejects scalar
opcodes on it.

## Verified gate (all green)

- `just ob89`, `just lint`: clean.
- Full unit suite (`./test/run_tests.sh`): all pass, incl. new v128
  round-trip + scalar-integrity unit test.
- Full 257-file conformance: **37171 passed / 0 failed / 0 STALL** (unchanged
  baseline — the widening perturbs nothing).
- Differential sweep: **3888 passed / 0 failed / 0 errors**; diff self-test
  passes.

## Why the `v128.const` / `v128.load` / `v128.store` slice is NOT in this merge

Implemented and locally verified (decode → validate → eval for sub-opcodes
0x00/0x0B/0x0C, plus a curated round-trip module), but enabling it in the
main build makes the real `simd_*.wast` corpus **hard-FAIL** instead of skip:

- SIMD files were clean *skips* only because the `0xFD` decode returned
  `UNSUPPORTED` (spec_driver skips unsupported modules). Once `v128.const`
  decodes, whole files proceed past decode into validation/instantiation.
- `simd_linking.wast` needs v128 **global const-expr initialisers and v128
  global imports** (instantiation support not present).
- `simd_const.wast` is mostly validation modules but also `assert_return
  (invoke ...) (v128.const ...)` — i.e. it needs **driver-side v128 result
  marshalling**, which is not implemented.

The repo discipline is `main` must always hold the full 257-file sweep at
zero FAIL/STALL, so a partial decode that flips files from skip to FAIL cannot
be merged. This matches the roadmap, which stages real-file SIMD conformance
only after kernels + driver support (B2/B3).

## Status: B1-b LANDED (this session, branch `feat/s3.1-simd-b1b` → `main`)

Re-enabling `v128.const`/`v128.load`/`v128.store` decode→validate→eval, and
treating v128 as a first-class value type across params/locals/globals and
global const-expr initialisers, kept the sweep green and flipped the two
const-only SIMD files from FAIL to real module-validation passes:

- `simd_linking.wast` (v128 global const-expr init + v128 global
  import/export): **3 passed / 0 failed / 0 skipped**.
- `simd_const.wast`: **373 passed / 0 failed / 385 skipped**. The 373 passes
  are module-load/validation commands (v128.const through function bodies,
  params, locals, globals, const-exprs). The 385 skips are the `assert_return
  (invoke ...) (v128.const ...)` value commands — spec_driver cannot yet
  serialize a v128 value (see below), so it skips them.
- Full 257-file conformance: **37460 passed / 0 failed / 0 STALL** (up from
  37171; no FAIL regression). Differential 3888/0 unchanged.

Why this stays green while B1's earlier partial attempt did not: v128 global
const-expr init runs through the iterative driver (`eval_expr_driver`), and
`check_const`/`is_const_instr` now admit `v128.const` (sub 0x0C) as a
constant-expression instruction, so the modules that previously FAILed with
"constant expression required" now instantiate.

## Next increment (B1-c): driver-side v128 value marshalling

The remaining `simd_const` skips are `assert_return (invoke ...) (v128.const
<lanes>)` / `assert_trap`-style value commands. To make them real assertions:

- `test/spec_driver.py value_token()` must serialize a JSON v128 value
  (`{"type":"v128","lane_type":...,"value":[...]}`) into a v128 byte token for
  all lane types (i8/i16/i32/i64/f32/f64 incl. float/nan lane literals, which
  must byte-match wat2wasm's own canonicalization of the same literal in the
  module body).
- `repl.c fmt_value`/arg parsing must print and accept v128 as a 16-byte token
  (`fmt_num` on vt num 0x7B).

That is independent of the C engine (which is byte-exact via the module
binary) and can land on its own. After B1-c the SIMD value-return asserts
become real; then B2 (lane kernels) un-skips the rest of the SIMD corpus
subset → full.
