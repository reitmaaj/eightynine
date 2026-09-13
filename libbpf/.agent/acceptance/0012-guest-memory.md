# 0012 - Acceptance: owned guest address space and memory resolver

*Acceptance criteria for the sandbox Phase 2 memory model. Each item is either
a behaviour the software MUST exhibit or a behaviour it MUST reject.*

## MUST

* An opaque `bpf_memory` MUST own a table of guest regions, each with a guest
  base, a length, a permission set (read and/or write), and host backing.
* A read-only region (input/constant) MUST accept reads and MUST reject writes.
* A read/write region (working memory / stack) MUST accept reads and writes.
* The resolver MUST return the host backing pointer only when the entire
  interval `[addr, addr + byte_count)` fits inside one region, the region
  permits the requested operation, and all lengths and offsets fit host
  indexing types.
* Host pointer arithmetic MUST occur only after the interval has been
  validated.
* Guest memory MUST be accessed bytewise with explicit little-endian
  semantics.
* Address zero MUST be reserved (unmapped).

## MUST NOT

* The constructor MUST NOT accept overlapping region mappings.
* The constructor MUST NOT accept a mapping whose `base + length` overflows the
  64-bit guest address range.
* The constructor MUST NOT accept a mapping that covers guest address zero, or
  more region entries than the table can hold.
* The resolver MUST NOT accept an access whose interval crosses a region
  boundary, underflows the region base, overflows the guest address range, or
  exceeds `region_length - delta` for its region.
* The resolver MUST NOT grant a write to a read-only region or a read to a
  region with no read permission.
* A rejected access MUST NOT read from or write to any host memory.
