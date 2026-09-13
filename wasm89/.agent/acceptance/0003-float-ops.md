# Acceptance: floating-point operations

*Specification reference: WebAssembly Core Spec 3.0, section 4.3.3, under
the deterministic profile.*

## MUST

* `fadd`/`fsub`/`fmul`/`fdiv`/`fsqrt` produce IEEE-754 round-to-nearest
  ties-to-even results on finite inputs.
* Any operation whose result is a NaN produces exactly the canonical
  positive NaN: 0x7FC00000 (f32) or 0x7FF8000000000000 (f64).
* `fmin(a, -inf) = -inf`, `fmin(a, +inf) = a`, `fmax(a, +inf) = +inf`,
  `fmax(a, -inf) = a`.
* `fmin(+0, -0) = -0` and `fmax(+0, -0) = +0`.
* `fmin`/`fmax` with any NaN operand yield the canonical NaN.
* `fneg`/`fabs`/`fcopysign` preserve NaN payloads and only alter the
  sign.
* `ftrunc` rounds toward zero and is correct for negative inputs.
* `fnearest(0.5) = +0`, `fnearest(-0.5) = -0`, `fnearest(1.5) = 2`,
  `fnearest(2.5) = 2` (ties to even), `fnearest(-2.5) = -2`,
  `fnearest(3.5) = 4`, `fnearest(inf) = inf`.
* `feq(+0, -0) = 1`; comparisons with a NaN operand are 0 except `fne`,
  which is 1.
* Canonical values hold, e.g.:
  * `f32` 1.0 / 3.0 = 0x3EAAAAAB;
  * `f32` sqrt(2.0) = 0x3FB504F3;
  * `f64` 1.0 / 3.0 = 0x3FD5555555555555.

## MUST NOT

* No operation MUST return a NaN other than the canonical positive NaN.
* `fmin` MUST NOT return +0 for opposite-sign zeros (must be -0);
  `fmax` MUST NOT return -0.
* `fceil`/`ffloor`/`ftrunc`/`fnearest` MUST NOT canonicalize infinities
  or change signed zeroes (e.g. `ftrunc(-0.5) = -0`).
* `fneg`/`fabs`/`fcopysign` MUST NOT canonicalize or drop a NaN payload.
* An arithmetic result MUST NOT be computed with double rounding or FMA
  contraction (each wasm instruction rounds exactly once).
