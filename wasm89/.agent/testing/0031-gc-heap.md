# Testing: GC — heap value model and object ops (S3.2.C2)

*BDD scenarios for the GC runtime: a heap of struct/array instances owned by
the store, a new reference kind for heap objects (and inline i31), and the
`0xFB` instruction family (`struct.new/get/set`, `array.new/get/set/len`,
`i31.new/get_s/get_u`, `ref.eq`, `ref.test`/`ref.cast` + null variants,
typed `ref.func`). Stakeholder value: story 0000. Design:
`.agent/design/0011-gc.md`. Type declarations (struct/array/recgroups) and
the `anyref`/`eqref`/`structref`/`arrayref`/`i31ref` hierarchy already
decode/validate at the type level.*

## GC-001 Heap instances are distinct, identity-preserving refs

SCENARIO: struct.new and array.new allocate
GIVEN `struct.new`/`array.new` with field/initializer values
WHEN evaluated
THEN they allocate a heap instance on the store arena and yield a reference
    to it
AND two allocations yield distinct references (identity), even for equal
    field values.

SCENARIO: i31 is an inline, equality-preserving reference
GIVEN `i31.new n`
WHEN evaluated
THEN it yields an i31 reference carrying the 31-bit value inline
AND two `i31.new` of the same value compare equal under `ref.eq`
AND `i31.get_s`/`i31.get_u` return the sign/zero-extended 32-bit value.

## GC-002 struct and array access

SCENARIO: struct.new/get/set by field
GIVEN a struct instance with typed, possibly packed (i8/i16) fields
WHEN `struct.get`/`struct.set` access a field
THEN the correct field value is returned/updated; packed fields are
    sign/zero-extended on get as declared
AND accessing a field through a null reference traps.

SCENARIO: array.new/get/set/len
GIVEN an array instance created with a size and initializer
WHEN `array.get`/`array.set`/`array.len` execute
THEN `array.len` returns the element count, `array.get`/`set` address the
    indexed element, and an out-of-range or null access traps.

## GC-003 casts and the reference hierarchy

SCENARIO: ref.eq, ref.test, ref.cast
GIVEN references over the `eqref`-derived hierarchy (struct/array/i31)
WHEN `ref.eq` compares two eqrefs
THEN it returns true iff they denote the same object/equal i31
AND `ref.test`/`ref.cast` (and null variants) check/convert a reference to
    a target heaptype, succeeding only when the runtime type is a subtype
    of the target.

SCENARIO: Subtyping is honoured at runtime
GIVEN a heap value typed at its concrete struct/array type
WHEN it is used where an `anyref`/`eqref`/`structref`-typed operand is
    expected
THEN it is accepted per the declared subtype relation, and a cast to an
    unrelated type fails.
