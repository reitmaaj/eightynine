# Testing: SIMD — differential (S3.1.B5)

*BDD scenarios for exercising SIMD in the differential harness (PLAN S3.1).
Design: `.agent/design/0008-differential.md`, `.agent/design/0010-simd.md`.
Differential is enabled only after the interpreter runs SIMD (decode→
validate→eval green) so a mismatch is a real finding.*

## SIM-012 v128 exact-bits compare across engines

SCENARIO: Deterministic SIMD results match the engines
GIVEN a SIMD program with integer and finite-float lane results
WHEN run under wasm89 and under the differential engines (wasmtime +
wasm-interp)
THEN the v128 result bit patterns compare exact (per `compare.py` exact-bits
    rule), lane-for-lane, with the engine SIMD feature flag enabled.

SCENARIO: NaN-lane rule applies
GIVEN a SIMD float op whose result lanes are NaN under the deterministic
    profile
WHEN compared lane-for-lane across engines
THEN NaN lanes are compared by the SIMD NaN rule (NaN≈NaN regardless of
    payload/sign; NaN-vs-finite is a FAIL), extending the scalar rule in
    `compare.py` to per-lane.

SCENARIO: No SIMD emit before engine parity
GIVEN the module generator before the engine SIMD flags/rule are wired
WHEN it runs
THEN it MUST NOT emit v128 programs
AND once wired, generator templates cover the implemented SIMD ops.

## SIM-013 Trap interchangeability

SCENARIO: SIMD traps are interchangeable
GIVEN a SIMD memory/trap case (e.g. oob lane load/store)
WHEN run under wasm89 and the engines
THEN a runtime failure on both is acceptable (rt≈rt) per the differential
    verdict table; only a pass-vs-fail or result mismatch is a finding.
