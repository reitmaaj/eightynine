# Acceptance: numeric conversions

*Specification reference: WebAssembly Core Spec 3.0, section 4.3.4.*

## MUST

* `i32.wrap_i64` returns the value modulo 2^32.
* `i64.extend_i32_s`/`extend_i32_u` sign-extend / zero-extend.
* `trunc_*` (to i32/i64, from f32/f64, signed/unsigned) traps on NaN,
  infinity, and out-of-range input, and otherwise returns the truncation
  toward zero.
* Unsigned `trunc_*` accepts values in (-1, 0), yielding 0.
* `trunc_sat_*` returns 0 for NaN; the saturated bound for infinities and
  out-of-range finite values; and the truncation otherwise. It never
  traps.
* `f32.convert_i32_s(0x80000000) = -2^31` (bit 0xCF000000);
  `f32.convert_i32_u(0xFFFFFFFF) = 2^32` (bit 0x4F800000);
  `f64.convert_i64_u(0xFFFFFFFFFFFFFFFF) = 2^64`
  (bit 0x43F0000000000000).
* `f64.promote_f32` is exact; `f32.demote_f64` rounds to nearest and a NaN
  result is canonical.
* `reinterpret` conversions are exact bit-pattern round trips.
* `extend8_s`/`extend16_s`/`extend32_s` sign-extend the low 8/16/32 bits
  (e.g. i32.extend8_s(0x80) = 0xFFFFFF80, i32.extend16_s(0x8000) =
  0xFFFF8000).

## MUST NOT

* `trunc_*` MUST NOT return a value for NaN, infinity, or out-of-range
  input.
* `trunc_sat_*` MUST NOT trap.
* Reinterpret conversions MUST NOT alter the bit pattern.
* Conversions MUST NOT round with anything other than round-to-nearest.
