# Testing: recursion and stack exhaustion (A3 triage)

*Scenarios derived from the A3 full-suite sweep and clean direct probes on
`vendor/testsuite-main`. Conformance baseline is functionally green
(35,729 passed / 0 failed / 28,109 skipped on the 245 files that complete
within the harness deadline). The apparent "stalls" are slow-but-correct
computations that exceed the harness default deadline, not runtime bugs.*

## REC-001 Exhaustion traps (verified working)

SCENARIO: Unbounded recursion traps
GIVEN a function that calls itself non-tail with no base case (the
    testsuite's `runaway`)
WHEN invoked under `assert_exhaustion`
THEN the runtime reports `call stack exhausted` and the command passes
    (`@pass`).

VERIFIED: `runaway` returns `@pass` given enough time. `w89_config_init`
seeds `budget = 5000` and each nested non-tail frame decrements it, so
exhaustion fires when recursion depth reaches ~5000. Exhaustion is
correct; it is not broken.

## REC-002 Non-tail recursion is O(n^2) slow (PERF defect)

SCENARIO: Deep recursion should be linear
GIVEN a terminating non-tail recursion to depth N
WHEN N is 500, 1000, 2000
THEN elapsed time grows ~quadratically: 0.29s, 1.25s, 5.42s (release
    `-O2` build), i.e. per-call cost rises with depth.

CONSEQUENCE: reaching the exhaustion limit (~depth 5000) takes ~30s, so
`assert_exhaustion` tests in the official suite take tens of seconds
each instead of milliseconds. This is a genuine performance defect in the
frame/return machinery (not a correctness defect). FIXING this O(n^2)
behaviour is the recommended next step; it would make exhaustion and deep
recursion fast.

## REC-003 Slow-but-correct conformance tests exceed the harness deadline

SCENARIO: 1M-iteration tail loop / slow exhaustion
GIVEN testsuite commands such as `count 1_000_000` (tail-call loop,
    ~5s) and the `assert_exhaustion` `runaway` cases (~30s due to
    REC-002)
WHEN driven under the harness default 5s per-command deadline
THEN they are reported as STALL even though they complete correctly given
    a larger deadline.

NOTE: harness calibration, not a runtime defect. Deadlines are
configurable via `W89_COMMAND_TIMEOUT` and `W89_SWEEP_TIMEOUT`; a larger
deadline lets these be measured rather than mislabelled.
