# Testing: SIMD — v128 value, const, load/store (S3.1.B1)

*BDD scenarios for introducing the v128 (128-bit) runtime value and the
`v128.const` / `v128.load` / `v128.store` family (PLAN S3.1). `v128` is
already a valtype byte `0x7B`; the work is the wide runtime payload plus
the `0xFD` SIMD decode prefix. Stakeholder value: story 0000. Design:
`.agent/design/0010-simd.md`. Currently SIMD modules decode-reject and are
counted as skips.*

## SIM-001 A v128 value carries 128 bits distinctly from scalars

SCENARIO: Wide value round-trips lane bytes
GIVEN a v128 value holding 16 bytes
WHEN it is constructed, copied, and stored to a local/global/operand
THEN all 128 bits round-trip unchanged
AND a v128 never aliases or truncates to a scalar i64 lane of another
    value in the same config value stack.

SCENARIO: v128 is a distinct operand type
GIVEN an operand typed v128 on the value stack
WHEN a scalar-consuming numeric instruction references it
THEN it is rejected by validation (type mismatch), never silently
    reinterpreted.

## SIM-002 `v128.const` decodes its 16 immediate bytes

SCENARIO: v128.const yields the given lanes
GIVEN a `v128.const` with 16 explicit lane bytes
WHEN decoded and evaluated
THEN it pushes one v128 whose bit pattern equals those 16 bytes exactly.

SCENARIO: Lane-immediate shape is validated
GIVEN SIMD instructions that carry lane-shape immediates (lane count/width)
WHEN the immediate is inconsistent with the required lane shape
THEN it is rejected at decode/validate rather than producing a partial
    result.

## SIM-003 `v128.load`/`v128.store` use memarg addressing

SCENARIO: Load and store a full vector from memory
GIVEN a memory location
WHEN `v128.load` executes it reads the address, effective-address-trap
    rules apply as for a scalar load of the equivalent width
THEN the loaded v128 equals the 16 bytes at the aligned address.

SCENARIO: Out-of-bounds access traps
GIVEN a `v128.load`/`v128.store` whose effective address range is out of
    bounds
WHEN it executes
THEN it traps exactly as the equivalent-width scalar access would, with the
    same `oob` precedence.
