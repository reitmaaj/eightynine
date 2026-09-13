# Acceptance: scalar integer operations

*Specification reference: WebAssembly Core Spec 3.0, section 4.3.2.*

## MUST

* All i32 ops take and return `w89_u32` bit patterns; all i64 ops take and
  return `w89_u64` bit patterns.
* `add`/`sub`/`mul` wrap modulo 2^N and never trap.
* `div_u`/`div_s`/`rem_u`/`rem_s` signal a trap (return 0) on divisor 0
  and on signed division overflow, and otherwise return 1.
* Signed division and remainder truncate toward zero; the remainder takes
  the sign of the dividend.
* `rem_s(-2^(N-1), -1)` returns 0 and does NOT trap.
* Shift counts are taken modulo N; a shift by a multiple of N leaves the
  value unchanged.
* `shr_s` fills vacated high bits with the original sign bit.
* Rotations rotate by `count mod N`; `rotl/rotr` by 0 leave the value
  unchanged.
* `clz`/`ctz` return N for a zero input.
* `eqz` returns 1 for zero and 0 otherwise.
* Signed comparisons use the signed (two's complement) interpretation;
  unsigned comparisons use the unsigned interpretation.
* The canonical identities hold, e.g.:
  * 0xFFFFFFFF + 1 = 0 (i32 add);
  * 0 - 1 = 0xFFFFFFFF (i32 sub);
  * 0x80000000 / 0xFFFFFFFF traps (i32 div_s, overflow);
  * -7 / 2 = -3 and -7 rem 2 = -1 (i32);
  * 0x80000000 shr_s 31 = 0xFFFFFFFF (arithmetic shift);
  * 0x80000000 rotl 1 = 1 (i32);
  * 0xFFFFFFFFFFFFFFFF + 1 = 0 (i64 add);
  * clz(0) = 32/64, ctz(0) = 32/64, popcnt(0) = 0.

## MUST NOT

* Division/remainder MUST NOT return a value when the divisor is 0.
* Signed division MUST NOT return a value when the quotient overflows
  (-2^(N-1) / -1).
* Shifts MUST NOT trap and MUST NOT be undefined for any count.
* Signed comparison MUST NOT be confused with unsigned comparison.
* The ops MUST NOT trap for ordinary wrap-around arithmetic.
