# Scenarios: streaming algebra and INET16 boundaries

## SCENARIO: streaming equals one-shot

GIVEN any byte string `x`
WHEN the checksum is computed as one shot, as `init; update(x); final`, as
two updates split at any position, or as one update per byte
THEN all results are identical

Exhaustive over every non-empty chunking for lengths 0..12
(`2^(n-1)` chunkings for length `n`), then deterministic randomized
partitions for larger inputs.

## SCENARIO: INET16 odd and even boundaries

GIVEN byte strings with even and odd totals
WHEN updates are partitioned as 1+1, 1+2, 2+1, 3+3, 1+1+1, and 5+2+7+1
THEN every partition equals the contiguous one-shot value, including
partitions that end between the two bytes of a word

## SCENARIO: INET16 verification property

GIVEN a byte string `data` with checksum `C`
WHEN `C` is serialized big-endian and included at an even offset
THEN the checksum over the whole stream is zero

- even `len(data)`: `checksum(data || be16(C)) == 0`
- odd `len(data)`: `checksum(data || 0x00 || be16(C)) == 0`

The zero pad is required for odd input: appending the checksum directly to
an odd-length payload would place the checksum word at an odd offset and the
sum would not be all ones.

## SCENARIO: length boundaries

GIVEN buffer lengths 0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128,
255, 256, 257, 1023, 1024, 1025, 4095, 4096, and 4097
WHEN each algorithm streams the buffer
THEN the result equals the reference model
