# Acceptance: GC — conformance (S3.2.C3)

*Relates to `.agent/testing/0032-gc-conformance.md`. Stakeholder value:
story 0000. Design: `.agent/design/0011-gc.md`.*

## MUST

* The upstream `gc/` testsuite MUST be vendored into `vendor/testsuite-main`
  so it is swept with the rest of the conformance set.
* Vendored GC modules MUST run through the driver as real assertions once
  their instructions are implemented (not decode-unsupported skips).
* The GC subset sweep MUST show zero FAIL and zero STALL across the vendored
  GC files, with genuine traps and rejections matching the reference.

## MUST NOT

* A command exercising only implemented GC features MUST NOT be skipped.
* The GC sweep MUST NOT hang or STALL (boundedness, acceptance `0012`).
* A skip MUST be confined to modules exercising explicitly by-design-
  excluded features.
* Implementing GC MUST NOT regress the scalar/SIMD/reference suites already
  green.

## Gate

* Vendored `gc/` subset: zero FAIL, zero STALL.
* Prior suites (scalar, ref, SIMD, exceptions) unchanged green.
