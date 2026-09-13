# Testing: bounded, observable conformance sweep (A2)

*BDD scenarios for making the full-suite driver observable and bounded.
The driver talks to one persistent wasm89 REPL process; a command is
"stalled" if the runtime neither answers `@...` nor closes within a
bounded time.*

## SWP-001 Per-file progress

SCENARIO: Long run reports progress
GIVEN a sweep over many testsuite files
WHEN the sweep runs
THEN it prints one progress line per file (name and aggregate so far)
    and flushes after each file
SO THAT an observer can see the run advancing rather than appearing
    frozen.

## SWP-002 Bounded per-command response

SCENARIO: Stalled command fails fast
GIVEN a single REPL command that the runtime does not answer within a
    bounded deadline
WHEN the driver sends it
THEN the driver stops waiting, reports the command as failed/errored
    (not silently hangs), and tears the runtime down
AND the driver can proceed to run a fresh runtime for subsequent files.

## SWP-003 Bounded per-file run

SCENARIO: Stalled file does not wedge the sweep
GIVEN a testsuite file whose driver run exceeds the per-file deadline
WHEN the sweep processes that file
THEN the file is recorded as an error and the sweep continues with the
    next file
SO THAT one pathological file cannot prevent the rest of the suite from
    being measured.

## SWP-004 Aggregate accounting

SCENARIO: Every file accounted for
GIVEN a completed sweep
WHEN it terminates
THEN every selected file is accounted for as passed, failed, skipped, or
    a reported per-file error
AND the final line reports total files, passed, failed, skipped.
