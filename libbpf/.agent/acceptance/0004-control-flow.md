# 0004 - Acceptance: control flow, CALL and EXIT

*Acceptance criteria for the control-flow milestone (RFC 9669 section 4.3).
Each item is either a behaviour the software MUST exhibit or a behaviour it
MUST reject.*

## MUST

* JA (JMP) MUST advance the program counter by `pc + 1 + offset`; JA (JMP32)
  MUST advance by `pc + 1 + imm`.
* Conditional jumps MUST branch by the offset relative to the instruction
  after the jump, and fall through by one otherwise.
* JMP comparisons MUST be 64-bit; JMP32 comparisons MUST be 32-bit, with
  signed comparisons sign-extending the 32-bit operands.
* EXIT MUST terminate the program (reported as RETURNED) at top level and
  return control to the caller inside a program-local function.
* CALL src_reg 1 MUST jump to a program-local function and push the return
  address; CALL src_reg 0 or 2 MUST report HOSTCALL with the helper id.
* The step budget MUST terminate an unbounded loop by reporting EXHAUSTED.

## MUST NOT

* The machine MUST NOT run past the end of the program; reaching a program
  counter past the last instruction MUST be reported as an error.
* The machine MUST NOT run forever; a finite budget MUST always bring it to a
  terminal status.
* A conditional jump MUST NOT be taken based on a 64-bit comparison when the
  instruction is JMP32, and vice versa.
