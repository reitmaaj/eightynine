# Design: saturating float-to-integer conversions in the evaluator

*Drives `.agent/testing+acceptance/0016-saturating-trunc.md` and stories
`0011`/`0012`. A small, isolated evaluator addition: the eight
`*_trunc_sat_*` instructions are already decoded (`decoder.c`), validated
(`validate.c`, `w89_tsat_tab`), and provided as pure primitives
(`numeric.c`), but `step_bulk` never dispatches their `0xFC` sub-opcodes
`0x00`-`0x07` and falls through to `crash("unsupported bulk instruction in
evaluator")` at `src/eval.c:2813`.*

## 1. Current state

- Decode: `decoder.c:2185/2701` accepts the `0xFC` prefix and stores
  `in.sub`.
- Validation: `validate.c:3258` `if (sub <= 0x07)` types the eight
  conversions via `w89_tsat_tab[]` (`validate.c:1977`):
  | sub | conversion |
  |---|---|
  | 0x00 | f32 -> i32 s |
  | 0x01 | f32 -> i32 u |
  | 0x02 | f64 -> i32 s |
  | 0x03 | f64 -> i32 u |
  | 0x04 | f32 -> i64 s |
  | 0x05 | f32 -> i64 u |
  | 0x06 | f64 -> i64 s |
  | 0x07 | f64 -> i64 u |
- Primitives (never trap; return the saturated value directly):
  `w89_i32_trunc_sat_f32_s/_u`, `w89_i32_trunc_sat_f64_s/_u` (return
  `w89_u32`), `w89_i64_trunc_sat_f32_s/_u`, `w89_i64_trunc_sat_f64_s/_u`
  (return `w89_u64`) in `numeric.c` around line 1104.
- Evaluator: all `0xFC` instructions route to `step_bulk`
  (`eval.c:4490`); `step_bulk` (`eval.c:2389`) handles memory/table
  sub-opcodes `0x08+` and defaults to `crash("unsupported bulk instruction
  in evaluator")` for sub `0x00`-`0x07`.

## 2. Change

In `step_bulk`, after the frame guard, dispatch sub-opcodes `0x00`-`0x07`:

```
if (sub <= 0x07) {
    /* pop the source float, compute the saturated integer, push it. */
    switch (sub) {
    case 0x00: f = pop_f32(c); push_u32(c, w89_i32_trunc_sat_f32_s(f)); break;
    case 0x01: f = pop_f32(c); push_u32(c, w89_i32_trunc_sat_f32_u(f)); break;
    case 0x02: d = pop_f64(c); push_u32(c, w89_i32_trunc_sat_f64_s(d)); break;
    case 0x03: d = pop_f64(c); push_u32(c, w89_i32_trunc_sat_f64_u(d)); break;
    case 0x04: f = pop_f32(c); push_u64(c, w89_i64_trunc_sat_f32_s(f)); break;
    case 0x05: f = pop_f32(c); push_u64(c, w89_i64_trunc_sat_f32_u(f)); break;
    case 0x06: d = pop_f64(c); push_u64(c, w89_i64_trunc_sat_f64_s(d)); break;
    default:   d = pop_f64(c); push_u64(c, w89_i64_trunc_sat_f64_u(d)); break;
    }
    code_consume(code);
    return W89_STEP_OK;
}
```

This mirrors the established non-saturating trunc pattern
(`eval.c:3414`-`3525`) but calls the trap-free saturating primitives. It
must sit in `step_bulk` (the single `0xFC` dispatcher) and therefore cannot
accidentally affect the trapping `0xA8+` non-saturating truncations handled
by `step_numeric`.

## 3. Rationale

- No decoder/validator change: those paths are already correct and tested
  (the conformance suite currently decodes and validates these but they
  crash only when executed).
- Single small site keeps the change auditable and C89-clean.
- Reuses the existing pure numeric primitives, avoiding duplicated float
  logic in the evaluator.

## 4. Out of scope
SIMD/v128 and GC (not decoded), exceptions differential (separate,
tooling-side work), and any change to trapping truncation semantics.
