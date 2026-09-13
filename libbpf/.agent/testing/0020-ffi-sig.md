# 0020 - Testing: FFI signature descriptors

*BDD scenarios for the typed effects interface descriptors. An import's
signature is an ordered list of argument kinds drawn from a small vocabulary.
Each kind consumes guest argument registers (a scalar or capability takes one;
an input/output buffer takes two: address and length/capacity). A signature is
valid only if its kinds are all legal and its total register use fits the five
guest argument registers.*

## 0020-001 Argument kinds

SCENARIO: A scalar or capability argument uses one register
GIVEN a signature of only u32/u64/i32/i64/capability/fixed-output arguments
WHEN its register use is computed
THEN each argument counts as one register.

SCENARIO: An input or output buffer argument uses two registers
GIVEN a signature containing an input or output buffer argument
WHEN its register use is computed
THEN each buffer argument counts as two registers.

## 0020-002 Register budget

SCENARIO: A signature within five registers is valid
GIVEN a signature whose total register use is at most five
WHEN it is checked
THEN it is accepted.

SCENARIO: A signature over five registers is rejected (unacceptable behaviour)
GIVEN a signature whose total register use exceeds five
WHEN it is checked
THEN it is rejected.

## 0020-003 Unknown kinds

SCENARIO: An unknown argument kind is rejected (unacceptable behaviour)
GIVEN a signature containing a kind outside the defined vocabulary
WHEN it is checked
THEN it is rejected.
