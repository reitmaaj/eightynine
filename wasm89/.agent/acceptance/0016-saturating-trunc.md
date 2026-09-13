# Acceptance: saturating float-to-integer conversion

*Acceptance criteria for making the eight `*_trunc_sat_*` instructions
(`0xFC` sub `0x00`-`0x07`) evaluable in wasm89. The decoder and validator
already handle them; only the evaluator dispatch in `step_bulk`
(`src/eval.c`) is missing.*

## MUST

* The evaluator MUST execute each of `i32.trunc_sat_f32_s`, `_u`,
  `i32.trunc_sat_f64_s`, `_u`, `i64.trunc_sat_f32_s`, `_u`,
  `i64.trunc_sat_f64_s`, `_u` and leave the specified integer result on the
  value stack.
* Each instruction MUST return `0` for any NaN operand.
* Signed variants MUST saturate negative out-of-range operands to the
  signed minimum and positive out-of-range operands to the signed maximum
  of the result type, and unsigned variants MUST saturate negative operands
  to `0` and positive out-of-range operands to the all-ones value of the
  result type.
* Fractional operands MUST be truncated toward zero before the range check.
* None of the eight instructions MUST trap on NaN, `±inf`, or out-of-range
  operands.
* None of the eight instructions MUST report "unsupported bulk instruction
  in evaluator".
* Results MUST match the reference semantics exercised by the `conversions`
  and `float_exprs` conformance suites.

## MUST NOT

* The evaluator MUST NOT route the saturating truncations through the
  trapping `invalid conversion to integer` path used by non-saturating
  truncations.
* This change MUST NOT alter the behaviour of non-saturating truncation,
  decode, or validation.
* The code MUST stay C89-clean (`just lint`, `just ob89`, `just test`) and
  MUST NOT introduce warnings.
