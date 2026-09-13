# Testing: SIMD — conformance (S3.1.B3)

*BDD scenarios for driving the 59 vendored `simd_*.wast` files (top level of
`vendor/testsuite-main`) to zero FAIL/zero STALL (PLAN S3.1). Stakeholder
value: story 0000. Design: `.agent/design/0010-simd.md`.*

## SIM-008 SIMD conformance modules run, not skip

SCENARIO: SIMD files reach pass/fail
GIVEN a SIMD conformance .wast whose instructions are implemented
WHEN it runs through the conformance driver
THEN its commands are evaluated as real assertions (pass or genuine fail)
    rather than being counted as decode-unsupported skips.

SCENARIO: Full SIMD suite green
GIVEN the 59 vendored `simd_*.wast` files covering const/load/store,
    integer+float lane ops, comparisons, shuffles, bitmask, conversions,
    dot, extmul, and saturating ops
WHEN the SIMD subset sweep runs
THEN each file passes with zero FAIL and zero STALL
AND genuine traps/rejections match the reference.

## SIM-009 Skips stay limited to by-design exclusions

SCENARIO: No spurious SIMD skips
GIVEN the SIMD suite under test
WHEN any assertion references only implemented SIMD features
THEN it MUST NOT be skipped
AND a skip is acceptable only where the module exercises an explicitly
    by-design-excluded feature (e.g. relaxed-SIMD lanes before B4, or a
    non-core host/thread feature).
