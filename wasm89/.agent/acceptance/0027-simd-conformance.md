# Acceptance: SIMD — conformance (S3.1.B3)

*Relates to `.agent/testing/0027-simd-conformance.md`. Stakeholder value:
story 0000. Design: `.agent/design/0010-simd.md`.*

## MUST

* The 59 vendored `simd_*.wast` files MUST pass through the conformance
  driver as real assertions once their instructions are implemented (not
  counted as decode-unsupported skips).
* The SIMD subset sweep MUST show zero FAIL and zero STALL across those
  files.
* Genuine traps and validation rejections MUST match the reference.

## MUST NOT

* A command exercising only implemented SIMD features MUST NOT be skipped.
* The SIMD sweep MUST NOT hang or STALL (per-file and per-command
  boundedness MUST hold, acceptance `0012`).
* A skip MUST be confined to modules exercising explicitly by-design-
  excluded features (e.g. relaxed-SIMD lanes before B4).
* Implementing SIMD MUST NOT regress the scalar conformance suite (the
  previously non-SIMD 198 files MUST stay green).

## Gate

* `simd_*.wast` subset: zero FAIL, zero STALL.
* Scalar subset and full existing sweep unchanged (no regression).
