# Testing: full memory/table/bulk/ref surface (Phase C)

*BDD scenarios for the Phase C milestone: every load/store form,
memory64/table64 index types, table.set/size/grow/fill/copy, memory.copy/
fill, data/elem segments, the non-GC ref instructions, ref-typed import
matching, and the conformance driver's named-module support.*

## MEM-001 Loads and stores

SCENARIO: Every load form
GIVEN a memory holding known bytes and functions using each of
    `i32.load`, `i32.load8_s/u`, `i32.load16_s/u`, `i64.load`,
    `i64.load8_s/u`, `i64.load16_s/u`, `i64.load32_s/u`, `f32.load`,
    `f64.load`
WHEN invoked with in-bounds addresses
THEN each returns the little-endian, correctly sign/zero-extended value.

SCENARIO: Every store form
GIVEN functions using `i32.store`, `i32.store8`, `i32.store16`,
    `i64.store`, `i64.store8/16/32`, `f32.store`, `f64.store`
WHEN invoked with in-bounds addresses
THEN the bytes are written little-endian (truncated to the packed width).

SCENARIO: Effective address
GIVEN a load/store with a nonzero offset and an address
WHEN executed
THEN the effective address is index + offset (no overflow in u64).

SCENARIO: Out-of-bounds
GIVEN a load/store whose access extends past the memory size (including
    exactly at the boundary)
WHEN executed
THEN evaluation traps with "out of bounds memory access".
GIVEN a store that traps
THEN no partial bytes are written.

SCENARIO: memory64 addressing
GIVEN a memory64 and a load/store with an i64 index
WHEN executed
THEN the effective address is computed as u64 and bounds-checked against
    the byte size.

## MEM-002 memory.size and memory.grow

SCENARIO: 64-bit size/grow
GIVEN a memory64
WHEN `memory.size` is executed
THEN the page count is returned as i64.
WHEN `memory.grow` grows within limits
THEN the old i64 size is returned and new pages are zeroed.
WHEN `memory.grow` exceeds the max or the address space (or realloc fails)
THEN -1 is returned as i64 and the memory is unchanged.

## MEM-003 memory.copy

SCENARIO: Copy ranges
GIVEN a memory with source and destination ranges in bounds (including
    overlapping, both d<=s and d>s)
WHEN `memory.copy` is executed
THEN the block is copied with memmove semantics.
GIVEN either range out of bounds (even with n = 0 and an out-of-bounds
    base)
THEN evaluation traps with "out of bounds memory access".
GIVEN distinct source and destination memories
THEN the copy crosses memories correctly.

## MEM-004 memory.fill

SCENARIO: Fill
GIVEN an in-bounds destination range
WHEN `memory.fill` is executed
THEN every byte in the range holds the fill byte.
GIVEN an out-of-bounds destination range
THEN evaluation traps with "out of bounds memory access".

## MEM-005 memory.init and data.drop

SCENARIO: Segment copy
GIVEN a passive data segment
WHEN `memory.init` is executed with in-bounds destination and segment
    ranges
THEN the bytes are copied from the segment (index roles and operand order
    per spec: dataidx via idx2, memidx via idx, operands n s d).
GIVEN an out-of-bounds destination or segment range (including n = 0 with
    an out-of-bounds base)
THEN evaluation traps with "out of bounds memory access".
SCENARIO: Segment drop
GIVEN `data.drop` on a segment
WHEN a later `memory.init` reads from it
THEN evaluation traps (the segment is empty), including the active segment
    dropped at instantiation.

## MEM-006 Multi-memory

SCENARIO: Distinct memories
GIVEN a module with several memories
WHEN `memory.init`/`memory.copy`/`memory.fill`/loads/stores address a
    non-default memory by index
THEN the correct memory is used (indices not aliased to the data index).

## TAB-001 table.get/set and bounds

SCENARIO: Table read/write
GIVEN a table
WHEN `table.get`/`table.set` are executed in bounds
THEN the slot is read/written.
GIVEN an out-of-bounds index
THEN evaluation traps with "out of bounds table access".

## TAB-002 table.size and table.grow

SCENARIO: Size
GIVEN a table32 and a table64
WHEN `table.size` is executed
THEN the size is returned as i32 / i64.
SCENARIO: Grow
GIVEN `table.grow` within the declared max
THEN the old size is returned and new slots hold the given ref.
GIVEN `table.grow` exceeding the max or the address space
THEN -1 is returned and the table is unchanged.

## TAB-003 table.fill and table.copy

SCENARIO: Fill
GIVEN an in-bounds destination range
WHEN `table.fill` is executed
THEN every slot holds the given ref.
GIVEN an out-of-bounds destination range
THEN evaluation traps with "out of bounds table access".
SCENARIO: Copy
GIVEN in-bounds overlapping source/destination ranges
WHEN `table.copy` is executed
THEN slots are copied with memmove semantics.
GIVEN either range out of bounds
THEN evaluation traps.

## TAB-004 table.init and elem.drop

SCENARIO: Segment copy
GIVEN a passive element segment
WHEN `table.init` is executed in bounds
THEN the slots are copied from the segment (elemidx via idx2, tableidx via
    idx, operands n s d).
GIVEN an out-of-bounds destination or segment range
THEN evaluation traps.
SCENARIO: Segment drop
GIVEN `elem.drop` on a segment
WHEN a later `table.init` reads from it
THEN evaluation traps.

## REF-001 Non-GC ref instructions

SCENARIO: ref.is_null
GIVEN a null and a non-null reference
WHEN `ref.is_null` is executed
THEN 1 and 0 are pushed.

SCENARIO: ref.eq
GIVEN two references of equal or different kind/identity
WHEN `ref.eq` is executed
THEN 1 iff both null, or both the same func/extern/exn instance.

SCENARIO: ref.as_non_null
GIVEN a non-null reference
WHEN `ref.as_non_null` is executed
THEN the reference is returned.
GIVEN a null reference
THEN evaluation traps with "null reference".

SCENARIO: br_on_null / br_on_non_null
GIVEN a null reference under `br_on_null`
THEN the branch is taken.
GIVEN a non-null reference under `br_on_null`
THEN the reference stays on the operand stack.
GIVEN a non-null reference under `br_on_non_null`
THEN the branch is taken with the reference on the stack.
GIVEN a null reference under `br_on_non_null`
THEN execution falls through.

## INST-012 Ref-typed import matching

SCENARIO: Importing ref-typed globals and tables
GIVEN a module exporting globals/tables whose element or value type is a
    type index (e.g. `(ref $t)`)
WHEN another module imports them with a structurally-equal declared type
THEN instantiation succeeds (no crash).
GIVEN a mismatched declared type
THEN instantiation fails with "incompatible import type ...".

## INST-013 Spectest table64

SCENARIO: table64 export
GIVEN the pre-registered "spectest" instance
WHEN its `table64` export is imported as `(table i64 0 funcref)`
THEN instantiation succeeds and the table is 10..20 funcref.

## DRV-001 Named module references

SCENARIO: Named invocation
GIVEN a .wast whose actions reference a module by `$name` (not `last`)
WHEN the driver translates the action
THEN the REPL resolves the module by name and the assertion runs against
    that instance.

## DRV-002 Out-of-scope proposal files

SCENARIO: Module-linking proposal
GIVEN a file with `module_definition`/`module_instance` commands
WHEN the driver processes it
THEN those commands are skipped, and any module/register that would depend
    on the skipped instances is also skipped (not failed).
