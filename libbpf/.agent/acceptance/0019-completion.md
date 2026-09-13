# 0019 - Acceptance: request completion and resume

*Acceptance criteria for completing a pending helper request. Each item is a
behaviour the software MUST exhibit or MUST reject.*

## MUST

* A machine in WAITING MUST accept one completion, set r0 to the supplied
  status, clear r1-r5, and resume at the instruction after the helper call
  (state RUNNING).
* Once resumed and run to EXIT, r0 MUST hold the completion status.

## MUST NOT

* A completion MUST NOT be accepted when the machine is RUNNING, RETURNED,
  TRAPPED, or EXHAUSTED (i.e. not WAITING).
* A second completion of the same request MUST NOT be accepted after the first
  has resumed the machine.
* A rejected completion MUST NOT change the machine's state or registers.
