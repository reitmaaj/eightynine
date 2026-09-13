# BDD scenarios: durability and clean reopen (L2)

Scenarios in this file drive `test/api/test_sync.c` and the smoke test. IDs
cross-reference `.agent/acceptance/0002-durability.md` and the normative
contract in `.agent/design/0002-durability-contract.md`. Crash variants
(D08–D10) land with the crash suites in L6.

## Logical visibility versus durability

- SCENARIO Read before sync: GIVEN an appended record WHEN read before
  `ledger89_sync` THEN the record is visible in-process (D01).
- SCENARIO Iterate before sync: GIVEN appended records WHEN iterated before
  `sync` THEN the exact sequence is visible (D02).
- SCENARIO Sync then reopen: GIVEN an append followed by `sync` WHEN the
  handle is closed and reopened THEN the record survives exactly (D03).
- SCENARIO Batch then one sync: GIVEN a batch appended and a single `sync`
  WHEN reopened THEN every record of the batch survives (D05).
- SCENARIO Many appends, one sync: GIVEN 100 appended records and one
  `sync` WHEN reopened THEN all 100 survive (D04).
- SCENARIO Repeated sync: GIVEN a clean ledger WHEN `sync` runs repeatedly
  THEN every call succeeds and nothing changes (D06).
- SCENARIO Sync empty ledger: GIVEN an empty ledger WHEN `sync` runs THEN
  it succeeds and the ledger stays empty (D07).
- SCENARIO Append after reopen: GIVEN a synced and reopened ledger WHEN a
  new record is appended and synced THEN the sequence continues (D03b).

## Appended but unsynced

- SCENARIO Crash with unsynced tail: GIVEN records A (synced) and B (not
  synced) WHEN the process crashes THEN recovery exposes A or A+B complete,
  never a partial B (D08; crash suite).
- SCENARIO Failed sync boundary: GIVEN a failed `sync` WHEN recovery runs
  THEN the durable boundary is the last successful sync (D10; fault suite).
