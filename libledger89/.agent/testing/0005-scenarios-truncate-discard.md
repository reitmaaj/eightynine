# BDD scenarios: truncation and discard (L5)

Scenarios in this file drive `test/api/test_truncate.c`,
`test/api/test_discard.c`, and `test/crash/test_truncate.c`. IDs
cross-reference `.agent/acceptance/0005-truncate-discard.md`.

## Suffix truncation

- SCENARIO No-op at the end: GIVEN records WHEN `truncate_after(last)` or a
  larger index runs THEN nothing changes (T01, T12).
- SCENARIO Truncate after first: GIVEN many records WHEN
  `truncate_after(first)` runs THEN exactly the first record remains (T02).
- SCENARIO Empty representation: GIVEN a fresh ledger WHEN
  `truncate_after(0)` runs THEN it is empty and the base is preserved (T03).
- SCENARIO Inside the active segment: GIVEN records in the active segment
  WHEN truncated mid-segment THEN only the kept prefix remains (T04).
- SCENARIO At a segment boundary: GIVEN several segments WHEN the target is
  exactly a segment's last index THEN the whole later segments are removed
  (T05, T07).
- SCENARIO Inside a sealed segment: GIVEN a sealed segment with several
  records WHEN truncated mid-segment THEN the segment is rewritten to the
  new end (T06).
- SCENARIO Remove several segments: GIVEN many sealed segments WHEN
  truncated early THEN every later segment is removed (T08).
- SCENARIO Append after truncation: GIVEN a truncated ledger WHEN new
  records are appended with formerly removed indices and different payloads
  THEN the new content is visible and the old content is gone (T09, T10).
- SCENARIO Repeated truncation: GIVEN an already truncated ledger WHEN the
  same truncation runs again THEN it is a no-op (T11).
- SCENARIO Below the base: GIVEN a discarded prefix WHEN truncating below
  `first_index - 1` THEN `LEDGER89_ERR_RANGE` (T13).
- SCENARIO Reopen after truncation: GIVEN a truncated ledger WHEN reopened
  THEN the exact kept prefix returns (T15).
- SCENARIO Canonical branch replacement: GIVEN `1A 2B 3C 4D` synced WHEN
  `truncate_after(2)` then append `3X 4Y` and sync THEN the ledger is
  `1A 2B 3X 4Y` with no trace of C or D.

## Prefix discard

- SCENARIO No-op at or before the first record: GIVEN records WHEN
  `discard_before(first_index)` runs THEN nothing changes (D01).
- SCENARIO Inside the active segment: GIVEN records in the active segment
  WHEN discarded from the middle THEN the suffix remains and the base
  advances (D02).
- SCENARIO Inside a sealed segment: GIVEN a sealed segment WHEN discarded
  from the middle THEN it is rewritten under its key with the new first
  index (D03).
- SCENARIO Across whole segments: GIVEN many sealed segments WHEN discarded
  past several THEN the wholly discarded segments are removed (D04).
- SCENARIO Discard all: GIVEN records WHEN `discard_before(last + 1)` runs
  THEN the ledger is empty and the new base is preserved across reopen
  (D05).
- SCENARIO Append after discard: GIVEN a discarded ledger WHEN a record is
  appended at the new base THEN it is visible (D06).
- SCENARIO Reopen after discard: GIVEN a discarded ledger WHEN reopened THEN
  the exact suffix returns with the advanced first index (D07).

## Crash behavior

- SCENARIO Crash during truncation: GIVEN a truncation over the model
  filesystem WHEN power is lost at any step and the ledger is reopened THEN
  the recovered state is a contiguous prefix (pre- or post-truncation)
  (C-T).
- SCENARIO Failed truncation: GIVEN an injected I/O failure WHEN truncation
  fails THEN the handle is faulted and a reopen recovers a legal state
  (C-T-IO).
