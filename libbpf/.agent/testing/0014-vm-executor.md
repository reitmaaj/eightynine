# 0014 - Testing: owned machine executor over a validated program

*BDD scenarios for the opaque `bpf_vm` executor. A machine is created over a
loaded `bpf_program` and an owned guest address space, and steps instruction by
instruction. Branches and local calls follow the targets resolved at load time
(measured in slots), so execution agrees with RFC 9669 across wide
instructions. Guest memory is reached only through the region resolver, and
the tested ALU and condition helpers are reused.*

## 0014-001 Machine states

SCENARIO: EXIT at the top level returns the machine
GIVEN a program that reaches EXIT
WHEN the machine is stepped
THEN it reports RETURNED and r0 holds the value set before EXIT.

SCENARIO: A returned machine stays terminal (unacceptable to continue)
GIVEN a machine that has reported RETURNED or TRAPPED
WHEN it is stepped again
THEN it reports the same terminal state and does not advance.

## 0014-002 Instruction budget

SCENARIO: An unbounded loop is terminated by the budget (unacceptable to run
          forever)
GIVEN a program that loops to itself
WHEN the machine is run with a finite budget
THEN it reports EXHAUSTED, and further steps stay EXHAUSTED.

## 0014-003 Branch resolution at runtime

SCENARIO: A branch crossing a wide instruction lands where slots dictate
GIVEN a conditional branch whose slot target crosses a wide instruction
WHEN the machine runs the program
THEN it returns the value placed after the resolved target, not the value
     reached by a naive compressed-index fall-through.

## 0014-004 Calling convention

SCENARIO: EXIT returns from a program-local call to the saved return point
GIVEN a program-local CALL and an EXIT in the callee
WHEN the machine steps
THEN it resumes at the instruction after the CALL.

## 0014-005 Register snapshot and entry context

SCENARIO: Registers are observable through a read-only snapshot
GIVEN a machine
WHEN the caller inspects a register
THEN it sees the guest's register value without mutating the machine.

SCENARIO: On entry r1/r2 carry the context and r10 the frame pointer
GIVEN a freshly created machine
WHEN the caller inspects r1, r2, and r10
THEN r1 is the input region base, r2 its length, and r10 the stack base.

## 0014-006 Memory through the resolver

SCENARIO: Stores and loads reach guest memory only when valid
GIVEN a machine whose program stores to and loads from its working region
WHEN it runs
THEN the load observes the stored value, and an out-of-range access traps.

SCENARIO: r10 may be read as a base but not written
GIVEN a program that uses r10 only as a base register
WHEN it runs
THEN the access resolves within the stack region.
