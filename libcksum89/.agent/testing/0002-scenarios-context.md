# Scenarios: context laws, representation, portability

## SCENARIO: final is observational

GIVEN an initialized and updated context
WHEN `final` is called twice
THEN both calls return the same value and the context bytes are unchanged

## SCENARIO: contexts fork by assignment

GIVEN a context `a` with a common prefix consumed
WHEN `b = a` and the two contexts receive different suffixes
THEN each branch equals the one-shot checksum of its own byte string

## SCENARIO: zero-length updates are neutral

GIVEN any byte string
WHEN `update(ctx, NULL, 0)` or any zero-length update is inserted at any
position
THEN the checksum is unchanged

## SCENARIO: init resets reused storage

GIVEN a context used for one byte string
WHEN `init` is called again and a second byte string is consumed
THEN the result equals a fresh context over the second byte string

## SCENARIO: representation independence

GIVEN an allocated backing array
WHEN every buffer is presented at offsets +0 through +7
THEN every result equals the reference model, with no word-alignment,
host-endian, or strict-aliasing dependency

## SCENARIO: value domains stay bounded

GIVEN any sequence of operations
WHEN a 32-bit context state or result is inspected on a platform whose
`unsigned long` is wider than 32 bits
THEN bits above bit 31 are zero; `cksum89_inet16_ctx.sum` is at most
`0xffff` and `has_pending` is 0 or 1

## SCENARIO: non-8-bit machine model is rejected

GIVEN a translation unit that overrides `CHAR_BIT` to a value other than 8
WHEN it includes `cksum89.h`
THEN compilation fails with the header's machine-model error
