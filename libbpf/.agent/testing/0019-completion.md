# 0019 - Testing: request completion and resume

*BDD scenarios for completing a pending helper request. While a machine is
WAITING on a helper call, the host may complete it once with a status; the
machine sets r0 to that status, clears the caller-clobbered registers r1-r5,
and resumes at the instruction after the call. Completing when the machine is
not waiting is rejected as stale.*

## 0019-001 Completion of a waiting machine

SCENARIO: A waiting machine resumes after completion
GIVEN a machine suspended on a helper call
WHEN the host completes it with a status
THEN the machine returns to RUNNING and resumes at the instruction after the
     call with r0 holding the completion status.

## 0019-002 Status convention

SCENARIO: r0 carries the completion status
GIVEN a completion status of zero (success) or non-zero (failure)
WHEN the machine resumes and exits
THEN r0 equals the completion status.

## 0019-003 Caller-clobbered registers cleared

SCENARIO: r1-r5 are cleared on resume
GIVEN a machine suspended on a helper call whose r1-r5 held arguments
WHEN it is completed and resumes
THEN r1-r5 read zero.

## 0019-004 Stale or duplicate completion is rejected (unacceptable behaviour)

SCENARIO: Completing a machine that is not waiting is rejected
GIVEN a machine that is RUNNING or RETURNED (not waiting)
WHEN the host attempts to complete it
THEN it is rejected and the machine is unchanged.

SCENARIO: A second completion of the same request is rejected
GIVEN a machine that has already been completed back to RUNNING
WHEN the host attempts to complete it again
THEN it is rejected.
