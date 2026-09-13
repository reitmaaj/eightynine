# BDD scenarios: corruption classification (L7)

Scenarios in this file drive `test/corrupt/test_corruption.c`. IDs
cross-reference `.agent/acceptance/0007-corruption.md`.

## Structural mutation sweep

- SCENARIO Sealed header mutation: GIVEN any single byte flipped in a sealed
  segment header WHEN reopened THEN `LEDGER89_ERR_CORRUPT` (X-H1).
- SCENARIO Sealed footer mutation: GIVEN any single byte flipped in a sealed
  segment footer WHEN reopened THEN `LEDGER89_ERR_CORRUPT` (X-H2).
- SCENARIO Sealed batch header mutation: GIVEN a flipped batch header WHEN
  the batch's record is read or the ledger iterated THEN
  `LEDGER89_ERR_CORRUPT` (X-H3).
- SCENARIO Sealed record mutation: GIVEN any byte flipped in a record header,
  payload, or record CRC WHEN that record is read THEN
  `LEDGER89_ERR_CORRUPT` (X-H4).
- SCENARIO Sealed batch footer mutation: GIVEN a flipped batch footer WHEN
  the ledger is iterated THEN `LEDGER89_ERR_CORRUPT`; reading an earlier
  record may still succeed (X-H5).
- SCENARIO Active header mutation: GIVEN any flipped active header byte WHEN
  reopened THEN `LEDGER89_ERR_CORRUPT` (X-H6).
- SCENARIO Active history mutation: GIVEN a flip in a complete batch that is
  followed by another complete batch WHEN reopened THEN
  `LEDGER89_ERR_CORRUPT` (X-H7).
- SCENARIO Active tail mutation: GIVEN a flip in the last complete batch
  WHEN reopened THEN the tail is discarded and the preceding prefix is
  exposed (X-H8).

## Semantic impossibilities

- SCENARIO Impossible payload length: GIVEN a record header claiming a
  payload beyond the record limit or beyond the segment WHEN read THEN
  `LEDGER89_ERR_CORRUPT` and no read past the bounds (X01, X02).
- SCENARIO Index regression, jump, or duplicate: GIVEN a record with a
  valid CRC but a non-contiguous index WHEN read THEN
  `LEDGER89_ERR_CORRUPT` (X03, X04, X05).
- SCENARIO Malformed batch count: GIVEN a batch header with count zero WHEN
  read THEN `LEDGER89_ERR_CORRUPT` (X06).
- SCENARIO Valid checksum, impossible range: GIVEN a valid-CRC header with
  nonzero flags or a footer with `last_index < first_index` WHEN reopened
  THEN `LEDGER89_ERR_CORRUPT` (X07).
- SCENARIO Garbage after the active tail: GIVEN random bytes appended after
  the last batch WHEN reopened THEN the garbage is truncated and the prefix
  survives (X08).
- SCENARIO Garbage inside the active prefix: GIVEN random bytes inserted
  between two complete batches WHEN reopened THEN
  `LEDGER89_ERR_CORRUPT` (X09).
- SCENARIO Random entire file: GIVEN a segment replaced by random bytes WHEN
  reopened THEN `LEDGER89_ERR_CORRUPT` (X10).
- SCENARIO Random single-byte fuzz: GIVEN any single random flip WHEN
  reopened THEN the result is `LEDGER89_OK` or `LEDGER89_ERR_CORRUPT`, and
  an opened ledger never crashes or invents a record.
