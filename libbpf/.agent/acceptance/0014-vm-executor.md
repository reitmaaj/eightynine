# 0014 - Acceptance: owned machine executor over a validated program

*Acceptance criteria for the opaque `bpf_vm` executor. Each item is a behaviour
the software MUST exhibit or MUST reject.*

## MUST

* A machine MUST be created over a loaded `bpf_program` and an owned guest
  address space, and MUST step instruction by instruction.
* EXIT at the top level MUST report RETURNED with r0 holding the guest result;
  EXIT inside a program-local call MUST return to the saved call point.
* A branch or local CALL MUST follow the target resolved at load time in
  64-bit slots, so execution agrees across wide instructions.
* ALU, condition, and memory semantics MUST reuse the tested helpers and the
  region resolver; the frame pointer r10 MUST NOT be written by the machine.
* On entry, r1/r2 MUST carry the input (context) base and length and r10 the
  stack base; other registers start zero.
* A finite budget MUST terminate an unbounded loop with EXHAUSTED.
* A RETURNED, TRAPPED, or EXHAUSTED machine MUST remain terminal across further
  steps.
* Registers MUST be observable through a read-only snapshot without mutating
  the machine.

## MUST NOT

* The machine MUST NOT write guest memory that the resolver denies.
* The machine MUST NOT run past a terminal state or past the end of the
  program.
* The machine MUST NOT compute a guest address with undefined behaviour; an
  out-of-range effective address MUST trap rather than touch host memory.
