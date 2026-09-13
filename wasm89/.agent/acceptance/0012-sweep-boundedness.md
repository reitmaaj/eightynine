# Acceptance: bounded, observable conformance sweep (A2)

*Relates to `.agent/testing/0012-sweep-boundedness.md`.*

## MUST

* `test/spec_sweep.py` MUST print a progress line for every file it
  processes and MUST flush its output so progress is visible during a
  long run.
* `test/spec_driver.py` MUST NOT block forever waiting for a reply to a
  single command: if a command is unanswered within the driver's bounded
  deadline, the driver MUST stop waiting, report the command (increment
  `failed` and print a distinct `stall` diagnostic), and tear down the
  runtime so later work is not attempted against a wedged process.
* `test/spec_sweep.py` MUST impose a bounded per-file timeout; a file
  whose driver run exceeds the timeout MUST be recorded as an error and
  MUST NOT prevent processing of subsequent files.
* The sweep's final line MUST report total files, passed, failed, and
  skipped, and MUST report every per-file error that occurred.
* A file whose commands all pass or skip MUST NOT be reported as an
  error, and its aggregate MUST be counted toward the totals.
* The driver MUST still classify a genuine trap/rejection/assertion
  failure as `failed` and an unsupported feature as `skipped`, unchanged
  from its prior behaviour.

## MUST NOT

* The driver MUST NOT hang on a non-answering command.
* A single stalled file MUST NOT wedge the whole sweep.
* The change MUST NOT alter the pass/fail/skip semantics for well-behaved
  commands: a correctly executing command that answers promptly MUST be
  classified exactly as before.
