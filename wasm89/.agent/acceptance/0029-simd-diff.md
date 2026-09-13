# Acceptance: SIMD — differential (S3.1.B5)

*Relates to `.agent/testing/0029-simd-diff.md`. Design: `.agent/design/0008-
differential.md`, `.agent/design/0010-simd.md`. Cross-reference: acceptance
`0015-differential.md`.*

## MUST

* SIMD differential is enabled only after the interpreter runs SIMD
  (decode→validate→eval green), so a mismatch is a real finding.
* Integer and finite-float SIMD lane results MUST compare exact-bits
  lane-for-lane against the engines, with the engine SIMD feature flag on.
* SIMD NaN lanes MUST be compared by the per-lane NaN rule (NaN≈NaN
  regardless of payload/sign; NaN-vs-finite is a FAIL), extending the
  scalar rule in `compare.py`.
* A runtime failure (trap) that both wasm89 and an engine report MUST be
  acceptable (rt≈rt); a pass-vs-fail or a result mismatch MUST be a finding.
* The generator MUST NOT emit SIMD programs until the engine flags/NaN rule
  are wired; once wired, templates MUST cover the implemented SIMD ops.

## MUST NOT

* SIMD MUST NOT be emitted while the engines would parse it as unsupported
  or while a determinism/NaN rule mismatch could make a false finding.
* A relaxed-SIMD op MUST NOT be differentially generated unless both sides
  share a matching deterministic rule.
* A scalar/non-SIMD differential mismatch MUST NOT be introduced or hidden
  by the SIMD work.

## Gate

* Differential SIMD self-test green; the seeded differential sweep
  including SIMD templates runs with zero mismatch; `diff-sweep` green.
