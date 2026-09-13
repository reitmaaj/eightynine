# 0015 - Acceptance: preserved-register frames for program-local calls

*Acceptance criteria for the machine's calling-convention frames. Each item is
a behaviour the software MUST exhibit or MUST reject.*

## MUST

* A program-local CALL MUST push a host-private frame slot recording the
  return instruction and the current r6-r9 and r10.
* EXIT from a callee MUST restore the caller's r6-r9 and r10 from that frame
  and resume at the recorded return instruction.
* r0 MUST carry the callee's return value into the caller; r1-r5 are
  caller-clobbered and need not survive a call.
* Nested calls MUST keep each level's saved registers independent.
* The machine MUST bound call depth; exceeding it MUST trap without overflowing
  host metadata.
* A frame slot reused by a later call MUST reflect the later call's saved
  values, not the earlier one's.

## MUST NOT

* The machine MUST NOT leave a caller's r6-r9 or r10 clobbered by a callee.
* The machine MUST NOT run past the fixed call-depth limit or read past the
  frame stack.
