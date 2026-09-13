# 0004 - Testing: control flow, CALL and EXIT

*BDD scenarios for the control-flow milestone (RFC 9669 section 4.3). Related
items are bundled.*

## 0004-001 Unconditional jump (JA)

SCENARIO: JA JMP skips by the 16-bit offset
GIVEN a program whose first instruction is JA with offset 2
WHEN the machine steps
THEN the program counter advances to instruction 3 (pc+1+2).

SCENARIO: JA JMP32 skips by the 32-bit immediate
GIVEN a program whose first instruction is JA JMP32 with imm 5
WHEN the machine steps
THEN the program counter advances to instruction 6 (pc+1+5).

## 0004-002 Conditional jumps

SCENARIO: JEQ branches when equal
GIVEN a JEQ instruction with equal operands
WHEN the machine steps
THEN the program counter jumps by the offset.

SCENARIO: JEQ falls through when unequal
GIVEN a JEQ instruction with unequal operands
WHEN the machine steps
THEN the program counter advances by one.

SCENARIO: JMP unsigned comparison uses 64-bit unsigned
GIVEN a JGT instruction with dst a large unsigned value and src a negative
      sign-extended immediate
WHEN the machine steps
THEN the unsigned comparison determines the branch.

SCENARIO: JMP32 signed comparison sign-extends 32-bit operands
GIVEN a JSLT JMP32 instruction comparing two 32-bit operands
WHEN the machine steps
THEN the signed 32-bit comparison determines the branch.

## 0004-003 EXIT

SCENARIO: EXIT at top level returns the program
GIVEN a program that reaches EXIT
WHEN the machine steps
THEN the machine reports RETURNED
AND r0 holds the value set before EXIT.

## 0004-004 Program-local CALL

SCENARIO: CALL src_reg 1 calls a local function and EXIT returns
GIVEN a program with a CALL (src_reg 1) to an offset and an EXIT in the
      called region
WHEN the machine steps
THEN the program counter jumps to the function
AND an EXIT inside returns to the instruction after the CALL.

## 0004-005 Helper CALL

SCENARIO: CALL src_reg 0 or 2 requests a helper
GIVEN a program with a helper CALL (src_reg 0)
WHEN the machine steps
THEN the machine reports HOSTCALL with the helper id from the immediate
AND the program counter points past the CALL.

## 0004-006 Step budget

SCENARIO: An unbounded loop is rejected by the budget (unacceptable behaviour)
GIVEN a program that jumps to itself (an infinite loop)
WHEN the machine is run with a finite budget
THEN it reports EXHAUSTED instead of running forever.

## 0004-007 Falling off the end

SCENARIO: Reaching past the last instruction is an error (unacceptable
          behaviour)
GIVEN a program with no EXIT
WHEN the machine steps past the final instruction
THEN it reports an error.
