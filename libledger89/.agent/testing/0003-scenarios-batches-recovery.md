# BDD scenarios: batches and recovery (L3)

Scenarios in this file drive `test/unit/test_model_fs.c` and
`test/crash/test_recovery.c`. IDs cross-reference
`.agent/acceptance/0003-batches-recovery.md`.

## Model filesystem contract

- SCENARIO Unsynced name: GIVEN a file created and written but never
  directory-synced WHEN power is lost THEN the file is gone (MR01).
- SCENARIO Synced name and bytes: GIVEN a file written, file-synced, and
  directory-synced WHEN power is lost THEN the file and bytes survive
  (MR02).
- SCENARIO Torn write: GIVEN a write larger than the injected tear WHEN it
  runs THEN only the prefix persists and the call reports failure (MR03).
- SCENARIO Rename durability: GIVEN a rename not followed by a directory
  sync WHEN power is lost THEN the old name is restored (MR04).

## Batch atomicity and recovery

- SCENARIO Synced prefix survives: GIVEN record 1 synced and record 2
  appended without sync WHEN power is lost THEN only record 1 recovers
  (R01).
- SCENARIO Process crash keeps page cache: GIVEN the same state WHEN the
  process exits without power loss THEN both records recover (R02).
- SCENARIO Torn append: GIVEN a mid-batch torn write WHEN append fails and
  the ledger is reopened THEN no partial record or batch is visible and the
  prior prefix is intact (R03).
- SCENARIO Garbage at tail: GIVEN arbitrary bytes appended after the last
  complete batch WHEN reopened THEN the bytes are treated as a torn tail and
  truncated (R04).
- SCENARIO Garbage inside history: GIVEN arbitrary bytes inserted between
  two complete batches WHEN reopened THEN `LEDGER89_ERR_CORRUPT` (R05).
- SCENARIO Torn header: GIVEN an active segment shorter than its header WHEN
  reopened THEN an empty ledger is recovered (R06).
- SCENARIO Idempotent recovery: GIVEN a recovered ledger WHEN opened and
  closed repeatedly THEN the visible state never changes (R07).
- SCENARIO Append after recovery: GIVEN a recovered prefix WHEN a new
  record is appended and synced THEN the sequence continues contiguously
  (R08).
