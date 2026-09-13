# 0008 - Testing: print module

*BDD scenarios for the refactored print/format module (extracted from the CLI
so its pure helpers are unit-testable). Related items are bundled.*

## 0008-001 Error names

SCENARIO: Every error code maps to a descriptive name
GIVEN each bpf_err value
WHEN bpf_err_name runs
THEN it returns a nonempty, distinct descriptive string.

## 0008-002 Regs byte

SCENARIO: The regs byte packs src<<4 | dst
GIVEN a source and destination register
WHEN bpf_regs_byte runs
THEN it returns `(src << 4) | (dst & 0x0F)`.

## 0008-003 Disassembly mnemonics

SCENARIO: ALU instructions disassemble with an op name and width suffix
GIVEN an ALU64 ADD instruction
WHEN bpf_dis_one runs
THEN the output contains `add64`.

SCENARIO: JMP instructions disassemble with their condition name
GIVEN a JSLT JMP32 instruction
WHEN bpf_dis_one runs
THEN the output contains `jslt32`.

SCENARIO: Memory instructions disassemble with mode and size
GIVEN an LDX MEM DW instruction
WHEN bpf_dis_one runs
THEN the output contains `memx.dw`.

## 0008-004 Conformance group names

SCENARIO: Groups are listed newline-separated with base32 always first
GIVEN a conformance bitmask with base32 and base64 set
WHEN bpf_groups_into runs
THEN the output is `base32\nbase64\n`.

## 0008-005 CLI still works

SCENARIO: The CLI produces identical output after refactor
GIVEN the refactored binary
WHEN `bpf run/dis/groups/version` run
THEN the output matches the pre-refactor behavior.
