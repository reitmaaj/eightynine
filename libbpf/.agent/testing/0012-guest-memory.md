# 0012 - Testing: owned guest address space and memory resolver

*BDD scenarios for the sandbox Phase 2 memory model. Each machine owns a
private guest address space built from a fixed, non-overlapping table of
permission-checked regions. Address zero is reserved unmapped. Every guest
memory access goes through a single resolver that validates the whole
interval before any host pointer arithmetic.*

## 0012-001 Region construction

SCENARIO: A region table is built from configuration
GIVEN region configuration with kinds, guest bases, lengths, and initial
      contents
WHEN the memory object is created
THEN it owns a region table with the requested guest bases and lengths.

SCENARIO: Overlapping mappings are rejected (unacceptable behaviour)
GIVEN two regions whose address intervals overlap
WHEN the memory object is created
THEN it is rejected and no memory object is produced.

SCENARIO: Arithmetic overflow in a mapping is rejected (unacceptable behaviour)
GIVEN a region whose base plus length overflows the 64-bit address range
WHEN the memory object is created
THEN it is rejected.

SCENARIO: Guest address zero is reserved (unacceptable behaviour)
GIVEN a region whose interval includes guest address zero
WHEN the memory object is created
THEN it is rejected.

SCENARIO: Too many regions are rejected (unacceptable behaviour)
GIVEN more region entries than the table can hold
WHEN the memory object is created
THEN it is rejected.

## 0012-002 Resolver

SCENARIO: A read inside a readable region resolves
GIVEN a guest address and byte count fully inside a read-only region
WHEN the resolver checks read permission
THEN it resolves the access to the region's host backing.

SCENARIO: A read into a write-only-free read-only region fails when write is
          required (unacceptable behaviour)
GIVEN a byte count requested with write permission in a read-only region
WHEN the resolver is called
THEN it is rejected.

SCENARIO: An interval crossing a region boundary is rejected (unacceptable
          behaviour)
GIVEN a byte count that starts in one region and extends into the next
WHEN the resolver is called
THEN it is rejected.

SCENARIO: An interval that underflows or overflows the address range is
          rejected (unacceptable behaviour)
GIVEN an address and byte count whose sum would overflow, or an address below
      the region base
WHEN the resolver is called
THEN it is rejected.

SCENARIO: The whole interval must fit one region
GIVEN `delta = addr - region_base` and `byte_count` such that
      `byte_count > region_length - delta`
WHEN the resolver is called
THEN it is rejected.

## 0012-003 Bytewise access

SCENARIO: Little-endian loads and stores use the resolver
GIVEN a resolved access
WHEN a bytewise little-endian load or store is performed
THEN host memory is touched only after resolution
AND a write to a read-only region is rejected.
