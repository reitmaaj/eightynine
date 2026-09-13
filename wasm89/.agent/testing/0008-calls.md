# Testing: calls, instantiation, imports (Phase B)

*BDD scenarios for the interpreter milestone Phase B: function calls and
frames, tail calls, instantiation, import resolution/matching, the
spectest host, and the minimal memory/table/segment/ref subset the Phase
B conformance set requires. Mirrors the reference interpreter's `eval.ml`
(Invoke/Frame cases) and the instantiation functions.*

## CALL-001 Invoke and frames

SCENARIO: Function invocation
GIVEN a decoded function with params `[i32]`, locals `[i64]`, result
    `[i32]`, and a body that returns its param
WHEN it is invoked with one argument
THEN the result is that argument and the non-param local holds its
    default value.

SCENARIO: Result merging
GIVEN a function whose body completes normally with the result on the
    operand stack
WHEN invoked
THEN the frame unwinds and the results are delivered to the caller.

## CALL-002 Return

SCENARIO: Return unwinds frames
GIVEN a callee that executes `return` inside a block with carried values
WHEN invoked
THEN the callee's frame unwinds and the carried values become the result
    without executing the rest of the body.

## CALL-003 Recursion

SCENARIO: Recursive call
GIVEN a function that calls itself (`fac`-style)
WHEN invoked with a small positive argument
THEN the result matches the numeric computation.

## CALL-004 CallIndirect

SCENARIO: Table dispatch
GIVEN a table whose slots hold function references of a declared type
WHEN `call_indirect` is executed with an in-range index
THEN the referenced function is invoked.
GIVEN an index into a null table slot (within bounds)
THEN evaluation traps with "uninitialized element N".
GIVEN an index at or beyond the table size (or a negative index)
THEN evaluation traps with "undefined element N".
GIVEN a slot whose function type does not match the expected type
THEN evaluation traps with "indirect call type mismatch, expected ...".

## CALL-005 CallRef

SCENARIO: Typed call
GIVEN `call_ref` on a null function reference
THEN evaluation traps with "null function reference".
GIVEN `call_ref` on a non-null function reference
THEN the function is invoked.

## CALL-006 Tail calls

SCENARIO: return_call
GIVEN `return_call` / `return_call_indirect` / `return_call_ref` in the
    tail position
WHEN evaluated
THEN the current frame is not re-entered; the callee's frame replaces it
    (`ReturningInvoke`) and the results flow to the original caller.

## CALL-007 Call budget

SCENARIO: Stack exhaustion
GIVEN a budget of zero at an `Invoke`
THEN evaluation fails with exhaustion "call stack exhausted".
GIVEN unbounded recursion exceeding the budget
THEN evaluation fails with exhaustion "call stack exhausted".

## CALL-008 Host functions

SCENARIO: Host function invocation
GIVEN a host function instance (e.g. spectest `print_i32`)
WHEN invoked with the matching argument count and types
THEN it runs (printing to stdout for the spectest prints) and returns the
    declared result.

## CALL-009 ref.null and ref.func

SCENARIO: Reference constants
GIVEN a `ref.null <heaptype>` const expression
WHEN evaluated
THEN it produces a null reference.
GIVEN a `ref.func x` const expression in a table/global/elem initializer
THEN it produces a function reference to the function at index x in the
    declaring module instance.

## INST-001 Instantiation

SCENARIO: Empty module
GIVEN a valid empty module
WHEN instantiated
THEN it succeeds with no exports.

SCENARIO: Exports
GIVEN a module exporting functions, globals, tables, memories
WHEN instantiated and exports are looked up by name and kind
THEN each resolves to the corresponding instance.

## INST-002 Global initializers

SCENARIO: Const expression evaluation
GIVEN globals whose initializers are `i32.const`/`i64.const`/`f32.const`/
    `f64.const`, `global.get` of an earlier global, `ref.null`, and
    `ref.func`
WHEN instantiated
THEN each global holds the evaluated value.

## INST-003 Tables and element segments

SCENARIO: Table allocation and elem init
GIVEN a table with an active element segment of `ref.func` expressions
WHEN instantiated
THEN the table has the declared size (defaulted to the segment length),
    is null elsewhere, and the segment slots hold the function
    references; the segment is then dropped.

## INST-004 Memories and data segments

SCENARIO: Active data segment
GIVEN a memory with an active data segment at an offset
WHEN instantiated
THEN the bytes are copied into the memory at the offset (bounded by the
    memory size), the offset expression is evaluated, and the segment is
    dropped.
GIVEN an active data segment whose range exceeds the memory
WHEN instantiated
THEN instantiation fails (the memory access is out of bounds).

## INST-005 Start function

SCENARIO: Start runs at instantiation
GIVEN a module with a start function that mutates a global
WHEN instantiated
THEN the start function has run (the global reflects its effect).
GIVEN a start function that calls an imported function
WHEN instantiated
THEN the imported function is invoked during instantiation.

## INST-006 Import resolution

SCENARIO: Unknown import
GIVEN an import whose module name is not registered (including
    "spectest") or whose export name is absent
WHEN instantiated
THEN instantiation fails with "unknown import ...".
GIVEN an import resolving to a registered module's export or a spectest
    export
THEN instantiation succeeds.

## INST-007 Import type matching

SCENARIO: Matching and mismatching import types
GIVEN an import whose declared type matches the actual instance type
    (functions: equal param/result types including cross-module
    structurally-equal types; globals: equal mutability and value type;
    memories/tables: limits subtyping where the actual range contains the
    expected)
THEN instantiation succeeds.
GIVEN an import with the wrong kind, a mismatched value type, a mutable
    global where immutable is required, or limits that do not contain the
    expected range
THEN instantiation fails with "incompatible import type for ...".

## INST-008 Register and cross-module imports

SCENARIO: Registering an instance
GIVEN an instantiated module is registered under a name
WHEN another module imports an export by that name
THEN the import resolves to the registered instance and shares its
    identity (a mutable global write through one module is visible
    through the other).

## INST-009 Memory size and grow

SCENARIO: memory.size and memory.grow
GIVEN a memory with an initial size
WHEN `memory.size` is executed
THEN the current page count is returned.
WHEN `memory.grow` grows within the declared max
THEN the previous size is returned and new pages are zero-initialized.
WHEN `memory.grow` would exceed the max (or the address space)
THEN `-1` is returned and the memory is unchanged.

## INST-010 Memory access bounds

SCENARIO: In-bounds and out-of-bounds
GIVEN a load/store within the memory size
THEN it succeeds (endianness per spec).
GIVEN a load/store at or beyond the memory size
THEN evaluation traps with "out of bounds memory access".

## INST-011 Spectest host

SCENARIO: Spectest module
GIVEN the pre-registered "spectest" instance
WHEN its exports are inspected
THEN `global_i32`/`global_i64` are 666, `global_f32`/`global_f64` are
    666.6, `memory` is 1 page (max 2), `table` is 10 funcref (max 20),
    and the `print*` functions are host functions that write bare lines
    to stdout (which the driver ignores, since command results are
    `@`-prefixed).
