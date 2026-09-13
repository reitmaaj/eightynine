# Testing: SIMD — relaxed SIMD (deterministic profile) (S3.1.B4)

*BDD scenarios for relaxed-SIMD ops under the project's deterministic
profile (PLAN S3.1). Stakeholder value: story 0000 (relaxed determinism).
Concept: `.agent/concept/0000-concept.md`; design: `.agent/design/0010-simd.md`.*

## SIM-010 Relaxed ops choose a fixed deterministic result

SCENARIO: Deterministic choice for each relaxed op
GIVEN a relaxed-SIMD op whose spec permits a set of acceptable results
    (e.g. relaxed madd/nmadd, relaxed trunc, relaxed lane-select/dot,
    fused-or-fma variants)
WHEN evaluated under the deterministic profile
THEN it returns one fixed, documented result from the acceptable set,
    identical every run and identical across builds
AND that fixed choice is recorded in the design doc (`.agent/design/0010-
    simd.md` deterministic rule).

SCENARIO: Relaxed results are internally reproducible
GIVEN the same relaxed op over the same inputs run repeatedly
WHEN compared lane-for-lane
THEN every run yields the identical v128 bit pattern (no dependence on CPU
    flags, scheduling, or host FP rounding).

## SIM-011 Relaxed NaN follows the deterministic rule

SCENARIO: Relaxed float lanes canonicalize NaN
GIVEN a relaxed float op that can produce NaN
WHEN evaluated
THEN NaN lanes are canonicalized per the same deterministic rule used by
    fixed SIMD and scalar floats.
