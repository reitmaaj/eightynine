# Testing: validator

*BDD scenarios for the validator milestone (spec ch3 + A.4).*

## VAL-001 Type canonical equality

SCENARIO: Structurally identical types
GIVEN two type indices whose rec groups have identical subtype lists
    (same finality, same supertypes, same comptypes, same rec-group
    positions for in-group references)
WHEN their canonical forms are compared
THEN they are equal.
GIVEN two types that differ in finality, supertype list, arity, or any
    comptype field
THEN they are not equal.

SCENARIO: Recursive types
GIVEN two rec groups with the same shape but different members
    (e.g. t0 = struct(ref t1), t1 = struct(ref t0) versus
    t2 = struct(ref t3), t3 = struct(ref t2))
WHEN in-group references are compared
THEN matching positions are equal and differing positions are not.
GIVEN a type whose in-group reference is compared against a cross-group
    reference (or vice versa)
THEN the comparison is not equal.

## VAL-002 Subtyping

SCENARIO: Abstract heaptype lattice
GIVEN the abstract heaptypes
THEN `eq <: any`, `struct <: any|eq`, `array <: any|eq`, `i31 <: any|eq`,
    `none <: t` iff `t <: any`, `nofunc <: t` iff `t <: func`,
    `noexn <: t` iff `t <: exn`, `noextern <: t` iff `t <: extern`,
    and concrete func/struct/array types are subtypes of the matching
    abstract heaptype.
GIVEN an unrelated abstract pair (e.g. `struct <: func`)
THEN matching fails.

SCENARIO: Deftype subtyping
GIVEN two distinct type indices a and b with identical canonical forms
THEN `a <: b`.
GIVEN a whose supertype list contains c with `c <: b`
THEN `a <: b`.
GIVEN no canonical equality and no matching supertype
THEN `a <: b` fails.
GIVEN `(ref null $a)` and `(ref $a)`-style nullability
THEN `null <: nonnull` holds and `nonnull <: null` fails.

## VAL-003 Type recursion rules

SCENARIO: Forward use and finality
GIVEN a subtype whose supertype index is not strictly before it
THEN validation fails with "forward use of type N in sub type definition".
GIVEN a subtype whose supertype is final
THEN validation fails with "sub type N has final super type M".
GIVEN a subtype whose comptype does not match its supertype
THEN validation fails with "sub type N does not match super type M".

## VAL-004 Limits and sizes

SCENARIO: Memory and table size bounds
GIVEN a memory with min above 2^16 pages (i32) or 2^48 (i64)
THEN validation fails with "memory size must be at most ...".
GIVEN a table with min above 2^32-1 (i32) or 2^64-1 (i64)
THEN validation fails with "table size must be at most ...".
GIVEN limits with min greater than max
THEN validation fails with "size minimum must not be greater than maximum".

## VAL-005 Constant expressions

SCENARIO: Const-expr instruction set
GIVEN a global/table/segment offset whose init uses only const,
    i32/i64 add/sub/mul, ref.null, ref.func, or global.get of an
    immutable global, and whose block type matches the expected type
WHEN validated
THEN it is accepted.
GIVEN an init containing any other instruction (e.g. i32.eqz, local.get)
THEN validation fails with "constant expression required".
GIVEN a const-expr whose result type mismatches the expected type
THEN validation fails with a type-mismatch message.
GIVEN a global.get of a mutable global in a const-expr
THEN validation fails.

## VAL-006 ref.func declared functions

SCENARIO: Declared function references
GIVEN a `ref.func` instruction whose index is not referenced by any
    global/table init, elem segment, or export
WHEN validated
THEN validation fails with "undeclared function reference N".
GIVEN a `ref.func` index that is exported, in an elem segment, or in a
    global/table init
THEN validation succeeds.

## VAL-007 Instruction typing

SCENARIO: Stack typing
GIVEN a function body whose instructions type-check against the
    declared types (locals, labels, results, operand stack) per spec 3
    rules
WHEN validated
THEN it is accepted.
GIVEN a body with an operand-type mismatch
THEN validation fails with "type mismatch: instruction requires [...] but
    stack has [...]".
GIVEN a body with an unknown local/label/function/global/table/memory/type
    index
THEN validation fails with "unknown local N" / "unknown label N" /
    "unknown function N" / "unknown global N" / "unknown table N" /
    "unknown memory N" / "unknown type N".
GIVEN an uninitialized non-defaultable local used before set
THEN validation fails with "uninitialized local".
GIVEN a `global.set` of an immutable global
THEN validation fails with "immutable global".
GIVEN a start function with parameters or results
THEN validation fails with "start function must not have parameters or
    results".
GIVEN a tag whose function type has a non-empty result
THEN validation fails with "non-empty tag result type".
GIVEN a block/if/loop/try_table whose body leaves the wrong stack
THEN validation fails with "type mismatch: block requires [...] but stack
    has [...]".
GIVEN a br_table whose labels have differing arities
THEN validation fails with a type-mismatch message.

## VAL-008 Memory and table operations

SCENARIO: Alignment and offset
GIVEN a load/store whose alignment exceeds the natural alignment
THEN validation fails with "alignment must not be larger than natural".
GIVEN an i32-memory access whose offset is at least 2^32
THEN validation fails with "offset out of range".
GIVEN a memory/table index that does not exist (multi-memory/table)
THEN validation fails with "unknown memory N" / "unknown table N".
GIVEN a memory.copy/init whose operand types or arity mismatch
THEN validation fails with a type-mismatch message.

## VAL-009 Select and references

SCENARIO: select_t and reference ops
GIVEN `select` whose operands are numeric/vector of one type
THEN it is accepted; otherwise validation fails with the reference-type
    message.
GIVEN `select_t` with more than one result type
THEN validation fails with "invalid result arity other than 1 is not
    (yet) allowed".
GIVEN `ref.is_null`/`br_on_null`/`br_on_non_null`/`ref.eq` on stacks
    whose top is not a reference type
THEN validation fails with the reference-type message.

## VAL-010 Accept and reject order

SCENARIO: Validation order
GIVEN a module with multiple defects
WHEN validated
THEN the first defect in the reference's order (types, imports, tags,
    funcs, memories, tables, globals, datas, elems, func bodies, start,
    exports) is reported.
GIVEN a module that decodes and validates
WHEN the CLI loads it
THEN it prints "ok".
GIVEN a module with duplicate export names
THEN validation fails with "duplicate export name ...".
