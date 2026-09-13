# 0013 - Acceptance: bytewise little-endian access over the resolver

*Acceptance criteria for the bytewise little-endian memory primitives. Each
item is a behaviour the software MUST exhibit or MUST reject.*

## MUST

* A load of width 1, 2, 4, or 8 MUST return the guest bytes assembled as an
  unsigned little-endian integer.
* A store of width 1, 2, 4, or 8 MUST write the value to the guest bytes
  least-significant byte first.
* Loads and stores MUST first resolve the exact byte interval through the
  region resolver, so host memory is touched only after the interval is
  validated.
* Bytewise assembly MUST NOT depend on host alignment or aliasing.
* A load MUST succeed in a readable region (including read-only regions).

## MUST NOT

* A store MUST NOT succeed in a read-only region.
* A load or store whose interval does not fit inside one permitted region
  MUST NOT touch any host memory and MUST report an error.
* An invalid width MUST be rejected.
