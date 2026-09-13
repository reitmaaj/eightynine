# 0013 - Testing: bytewise little-endian access over the resolver

*BDD scenarios for the bytewise little-endian loads and stores the executor
uses. Every typed access first resolves the exact byte interval through the
region resolver, so a rejected access never touches host memory. Bytewise
assembly avoids host alignment and aliasing hazards.*

## 0013-001 Little-endian loads

SCENARIO: A byte/word/double-word load reads little-endian
GIVEN bytes in a readable region and a requested width of 1, 2, 4, or 8
WHEN a little-endian load is performed at the region base
THEN it returns the byte-assembled value.

SCENARIO: A load from a read-only region succeeds
GIVEN a read-only region
WHEN a load is performed
THEN it returns the region's bytes.

## 0013-002 Little-endian stores

SCENARIO: A byte/word/double-word store writes little-endian
GIVEN a writable region and a value
WHEN a store of width 1, 2, 4, or 8 is performed
THEN the region bytes encode the value least-significant byte first.

## 0013-003 Permission and boundary denial

SCENARIO: A store to a read-only region is denied (unacceptable behaviour)
GIVEN a read-only region
WHEN a store is attempted
THEN it is rejected and no region byte changes.

SCENARIO: An access that would cross the region end is denied (unacceptable
          behaviour)
GIVEN an address and width whose interval would cross the region boundary
WHEN a load or store is attempted
THEN it is rejected and no host memory is touched.
