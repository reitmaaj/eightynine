# BDD scenarios: format and checksums (L0)

Scenarios in this file drive `test/unit/test_crc.c` and
`test/unit/test_format.c`. IDs cross-reference
`.agent/acceptance/0000-format.md`. The byte layouts are fixed in
`.agent/design/0001-format-v1.md`.

## CRC-32C

- SCENARIO Empty input: GIVEN zero bytes WHEN checksummed THEN the result is
  `0x00000000` (F01).
- SCENARIO Standard vector: GIVEN the ASCII bytes `123456789` WHEN
  checksummed THEN the result is `0xE3069283` (F02).
- SCENARIO Zero vector: GIVEN 32 zero bytes WHEN checksummed THEN the result
  is `0x8A9136AA` (F03).
- SCENARIO Chunk independence: GIVEN a buffer split at every possible point
  WHEN checksummed incrementally THEN the result equals the one-shot
  checksum (F04).

## Scalar codecs

- SCENARIO Little-endian order: GIVEN a value WHEN encoded THEN the first
  byte is the least significant byte and decoding returns the value (F05).

## Segment header

- SCENARIO Canonical bytes: GIVEN `first_index = 1`, zero flags WHEN encoded
  THEN the 32 bytes match the documented layout and the CRC field validates
  (F06).
- SCENARIO Round trip: GIVEN arbitrary valid fields WHEN encoded and decoded
  THEN the fields are unchanged (F07).
- SCENARIO Bad magic: GIVEN a mutated magic WHEN decoded THEN
  `LEDGER89_ERR_CORRUPT` (F08).
- SCENARIO Bad version: GIVEN `format_version = 2` WHEN decoded THEN
  `LEDGER89_ERR_CORRUPT` (F09).
- SCENARIO Bad header CRC: GIVEN any single-byte mutation before the CRC
  field WHEN decoded THEN `LEDGER89_ERR_CORRUPT` (F10).

## Record header

- SCENARIO Round trip: GIVEN index, tag, size, flags WHEN encoded and
  decoded THEN the fields are unchanged (F11).
- SCENARIO Oversized payload: GIVEN `payload_size > 16 MiB` WHEN decoded
  THEN `LEDGER89_ERR_CORRUPT` (F12).
- SCENARIO Reserved bits: GIVEN nonzero flags or reserved bytes WHEN decoded
  THEN `LEDGER89_ERR_CORRUPT` (F13).

## Batch header and footer

- SCENARIO Round trip: GIVEN a valid batch header WHEN encoded and decoded
  THEN the fields are unchanged (F14).
- SCENARIO Impossible length: GIVEN `batch_bytes` smaller than the minimum
  batch WHEN decoded THEN `LEDGER89_ERR_CORRUPT` (F15).
- SCENARIO Footer round trip: GIVEN count and last index WHEN encoded and
  decoded THEN the fields and CRC field are unchanged (F16).

## Sealed segment footer

- SCENARIO Round trip: GIVEN last index, count, digest, body size WHEN
  encoded and decoded THEN the fields are unchanged (F17).
- SCENARIO Bad footer CRC: GIVEN any single-byte mutation before the CRC
  field WHEN decoded THEN `LEDGER89_ERR_CORRUPT` (F18).

## Width conversion

- SCENARIO Unrepresentable index: GIVEN a 64-bit value above `ULONG_MAX`
  WHEN converted to `ledger89_index` THEN `LEDGER89_ERR_RANGE` and the
  output is untouched (F19).
- SCENARIO Byte-exact re-encode: GIVEN a decoded structure WHEN re-encoded
  THEN the bytes are identical to the input (F20).
