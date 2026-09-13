# Acceptance: GC — heap value model and object ops (S3.2.C2)

*Relates to `.agent/testing/0031-gc-heap.md`. Stakeholder value: story 0000.
Design: `.agent/design/0011-gc.md`.*

## MUST

* `struct.new`/`array.new` MUST allocate a heap instance on the store arena
  and yield a distinct reference; two allocations MUST be distinct even for
  equal field values (identity).
* `i31.new` MUST carry its 31-bit value inline; equal `i31.new` values MUST
  be `ref.eq`-equal; `i31.get_s`/`i31.get_u` MUST return the correct
  sign/zero-extended 32-bit value.
* `struct.get`/`struct.set` MUST access the correct field, sign/zero-
  extending packed (i8/i16) fields as declared.
* `array.len` MUST return the element count; `array.get`/`array.set` MUST
  address the indexed element.
* `ref.eq` MUST return true iff the operands denote the same object or equal
  i31.
* `ref.test`/`ref.cast` (and null variants) MUST succeed only when the
  runtime type is a subtype of the target heaptype.
* A null or out-of-range heap access MUST trap.

## MUST NOT

* A heap reference MUST NOT alias another object, be reused after a cast
  to an unrelated type, or yield a pointer to freed/invalid storage.
* `ref.eq` MUST NOT conflate distinct objects merely because their fields
  are equal.
* `i31` MUST NOT exceed 31 bits or sign/zero-mis-extend on get.
* A packed-field write MUST NOT read/write outside its byte width or corrupt
  adjacent fields.
* Null handling MUST NOT be skipped: `struct.get`/`array.get`/`set` on null
  and `array` out-of-range MUST trap, not silently read/write.
* The scalar/ref/exceptions suites MUST stay green; the value model widening
  (new heap refkind + arena) MUST NOT change existing reference behaviour.

## Gate

* Unit tests for every heap op and its branches (identity, packed fields,
  i31, null/range traps, casts, subtyping) pass; `just lint ob89 test`
  green.
