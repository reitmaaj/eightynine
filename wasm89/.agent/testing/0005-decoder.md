# Testing: binary decoder

*BDD scenarios for the decoder milestone (spec ch5).*

## DEC-001 Module frame

SCENARIO: Magic and version
GIVEN a buffer shorter than 4 bytes or a wrong magic
WHEN the module is decoded
THEN the decode fails with "unexpected end" / "magic header not detected".
GIVEN a valid magic but wrong or truncated version
THEN the decode fails with "unknown binary version" / "unexpected end".
GIVEN a valid header and no sections
THEN the decode succeeds with an empty module.

## DEC-002 Section framework

SCENARIO: Section ordering and sizing
GIVEN sections appearing in increasing id order (custom sections anywhere,
    data count between element and code)
WHEN decoded
THEN the module decodes.
GIVEN a repeated section id, an id out of order, or an id after the data
    count whose own id is smaller
THEN the decode fails with "malformed section id".
GIVEN a section whose declared size does not match its content
THEN the decode fails with "section size mismatch".
GIVEN a section whose size extends past the end of the module
THEN the decode fails with "unexpected end".
GIVEN a truncated section's content
THEN the decode fails with "unexpected end of section or function".
GIVEN stray bytes after the last section
THEN the decode fails.

## DEC-003 Types

SCENARIO: Type encodings
GIVEN function types, struct types (with counted fields), array types
    (single field), sub types (final or not, with super types), recursive
    types (0x4E lists), and the unary shorthands
WHEN decoded
THEN the corresponding rectype list is produced.
GIVEN a reserved value type byte, a bad mutability byte, or a truncated
    type
THEN the decode fails with "malformed value type" / "malformed
    mutability" / "unexpected end of section or function".
GIVEN a ref/heap type whose encoded value is a negative s33
THEN the decode fails with "malformed reference type".

## DEC-004 Limits

SCENARIO: Limits
GIVEN limits flags 0x00/0x01 (i32) or 0x04/0x05 (i64, memory64) with
    min <= max
WHEN decoded
THEN a limits record is produced.
GIVEN any other flag or min > max
THEN the decode fails with "malformed limits flags" / "size minimum must
    not be greater than maximum".

## DEC-005 Sections

SCENARIO: Imports, exports, code, data
GIVEN imports of each kind (func/table/memory/global/tag)
WHEN decoded
THEN import records are produced; an unknown kind fails with "malformed
    import kind".
GIVEN exports with duplicate names or an unknown kind
THEN the decode fails with "duplicate export name" / "malformed export
    kind".
GIVEN function and code sections whose counts differ
THEN the decode fails with "function and code section have inconsistent
    lengths".
GIVEN a data count section whose value differs from the data section
    length
THEN the decode fails with "data count and data section have
    inconsistent lengths".
GIVEN a local declaration run whose total exceeds 2^32
THEN the decode fails with "too many locals".
GIVEN element segments of each flag form (0-7) and data segments of each
    flag form (0-2)
WHEN decoded
THEN segment records are produced; a bad elemkind byte fails with
    "malformed reference type".
