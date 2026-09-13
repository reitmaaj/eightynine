# 0015 - Testing: preserved-register frames for program-local calls

*BDD scenarios for the machine's calling-convention frames. The guest ABI keeps
r0 as the return value, r1-r5 as caller-clobbered arguments, r6-r9 as
preserved across calls, and r10 as the VM-managed frame pointer. The machine
keeps each activation's return address and saved r6-r9/r10 in host-private
frame slots, restores them on return, and bounds the call depth.*

## 0015-001 Preserved registers across a call

SCENARIO: r6-r9 set before a call are restored after the callee clobbers them
GIVEN a caller that sets r6-r9, calls a function, and then reads r6-r9 back
AND a callee that overwrites r6-r9 and returns
WHEN the machine runs
THEN the caller observes its original r6-r9 values restored after the call.

SCENARIO: r10 (the frame pointer) is restored after a call
GIVEN a caller whose r10 is modified by the callee
WHEN the callee returns
THEN the caller observes its original r10 value.

## 0015-002 Argument and return registers

SCENARIO: r0 carries the callee return value and r1-r5 are caller-clobbered
GIVEN a callee that sets r0 and clobbers r1
WHEN the callee returns
THEN the caller reads r0 from the callee while its preserved registers stay
     intact.

## 0015-003 Nested calls

SCENARIO: Nested calls preserve each level's registers
GIVEN an outer function that calls an inner function, both using r6-r9
WHEN control unwinds
THEN each level observes its own saved register values.

## 0015-004 Call depth bound

SCENARIO: Call depth beyond the limit is rejected (unacceptable behaviour)
GIVEN a chain of calls deeper than the fixed depth limit
WHEN the machine steps into the excess call
THEN it traps rather than overflowing host metadata.

## 0015-005 Frame reuse

SCENARIO: A returned frame's saved registers are overwritten by the next call
GIVEN two sequential calls at the same depth reusing a frame slot
WHEN the second call stores its own r6-r9
THEN the restored values reflect the second call, not the first.
