# Acceptance: SIMD — relaxed SIMD (deterministic profile) (S3.1.B4)

*Relates to `.agent/testing/0028-simd-relaxed.md`. Stakeholder value: story
0000. Concept: `.agent/concept/0000-concept.md`; design:
`.agent/design/0010-simd.md`.*

## MUST

* Every relaxed-SIMD op MUST return one fixed, documented result chosen
  from its acceptable set, recorded in `.agent/design/0010-simd.md`.
* Relaxed results MUST be byte-for-byte reproducible across runs and across
  builds (no dependence on host CPU flags, scheduling, or FP rounding
  mode).
* Relaxed float ops that can produce NaN MUST canonicalize NaN lanes under
  the same deterministic rule as fixed SIMD and scalar floats.

## MUST NOT

* A relaxed op MUST NOT return different acceptable results across runs or
  leave an engine/host-dependent choice in effect.
* The deterministic choice MUST NOT violate the reference's set of
  acceptable results for that op.
* Relaxed SIMD MUST NOT be enabled for the differential harness until a
  matching deterministic rule exists on both sides (see acceptance `0029`).

## Gate

* Unit tests asserting the fixed result and cross-run reproducibility pass.
* Relaxed-SIMD conformance modules that permit multiple results pass via
  the deterministic choice (module either matches the fixed lane result or
  is a documented by-design exclusion only where the op is out of scope).
