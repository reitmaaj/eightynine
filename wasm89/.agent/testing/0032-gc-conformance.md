# Testing: GC — conformance (S3.2.C3)

*BDD scenarios for driving the GC conformance suite to zero FAIL/zero STALL
(PLAN S3.2). Requires vendoring the upstream `gc/` testsuite into
`vendor/testsuite-main` (approved). Stakeholder value: story 0000. Design:
`.agent/design/0011-gc.md`.*

## GC-004 Vendored GC modules run, not skip

SCENARIO: GC conformance modules execute
GIVEN the vendored GC testsuite modules (struct/array/i31 construction and
    access, ref.eq/test/cast, typed ref.func, the reference type hierarchy
    and subtyping)
WHEN run through the conformance driver
THEN each implemented instruction's assertions are evaluated as real
    pass/fail rather than decode-unsupported skips.

SCENARIO: GC suite green
GIVEN the vendored GC conformance set
WHEN the GC subset sweep runs
THEN each file passes with zero FAIL and zero STALL, and genuine
    traps/rejections match the reference.

## GC-005 Skips stay limited to by-design exclusions

SCENARIO: No spurious GC skips
GIVEN the GC suite under test
WHEN any assertion exercises only implemented GC features
THEN it MUST NOT be skipped
AND a skip is acceptable only for a module exercising an explicitly
    by-design-excluded feature (non-core host/thread/component features).
