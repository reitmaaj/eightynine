# BDD scenarios: rotation and sealed immutability (L4)

Scenarios in this file drive `test/api/test_rotate.c`. IDs cross-reference
`.agent/acceptance/0004-rotation.md`.

## Explicit rotation

- SCENARIO Rotate empty: GIVEN an empty active segment WHEN rotated THEN it
  is a no-op and no sealed segment appears (R01).
- SCENARIO Rotate after one record: GIVEN one record WHEN rotated THEN one
  sealed segment exists and the record remains readable (R02).
- SCENARIO Rotate after many records: GIVEN many records WHEN rotated THEN
  the exact sequence is preserved (R03).
- SCENARIO Rotate twice: GIVEN an empty active segment WHEN rotated twice
  THEN the second rotation is a no-op (R10).
- SCENARIO Append after rotation: GIVEN a sealed segment WHEN a new record is
  appended THEN it lands in the new active segment (R11).
- SCENARIO Many rotations: GIVEN repeated append/rotate cycles THEN each
  record occupies exactly one sealed segment (R09).

## Automatic rotation

- SCENARIO Record threshold: GIVEN `max_segment_records = 3` WHEN a fourth
  record is appended THEN the first three are sealed first (R04, R05).
- SCENARIO Byte threshold: GIVEN a byte target WHEN the next batch would
  exceed it THEN the active segment is sealed first (R06, R07).
- SCENARIO Oversized record: GIVEN one record larger than the byte target
  WHEN appended to an empty active segment THEN it occupies its own segment
  (R08).

## Sealed immutability and cross-segment access

- SCENARIO Sealed bytes never change: GIVEN a sealed segment WHEN more
  records are appended and rotated THEN the sealed file bytes are identical
  (R03b).
- SCENARIO Read across boundaries: GIVEN records in several segments WHEN
  each index is read THEN the exact record returns (R12).
- SCENARIO Iterate across many boundaries: GIVEN 100 single-record segments
  WHEN iterated THEN all 100 records appear in ascending order (R13).
- SCENARIO Reopen after rotation: GIVEN sealed and active segments WHEN
  reopened THEN the exact sequence is restored (R15).
