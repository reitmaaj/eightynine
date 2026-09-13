# Acceptance criteria: batches and recovery (L3)

These criteria trace to `.agent/testing/0003-scenarios-batches-recovery.md`
and are enforced by `test/unit/test_model_fs.c` and
`test/crash/test_recovery.c`.

## Must exhibit (exhibit)

- The model filesystem MUST lose unsynced bytes and names on power loss and
  MUST keep file-synced bytes and directory-synced names.
- A write torn by injected power loss MUST persist only its prefix and MUST
  report failure.
- After a crash, recovery MUST expose exactly the durable prefix: a complete
  batch either fully survives or is absent.
- Arbitrary trailing bytes after the last complete batch MUST be discarded
  as a torn tail; the preceding records MUST remain byte-identical.
- Recovery MUST be idempotent across repeated open/close cycles.
- After recovery, appending MUST continue at `last_index + 1` and a
  subsequent sync MUST make it durable.

## Must reject / fail safely (reject)

- Arbitrary bytes inserted between two complete batches MUST yield
  `LEDGER89_ERR_CORRUPT`; recovery MUST NOT skip them or invent records.
- A torn append MUST NOT become visible, and the handle MUST be faulted so
  no further mutation can create a gap.
- A torn segment header MUST NOT be treated as history; it MUST be
  recreated as an empty active segment.
- No crash point may expose a partial batch, a gap, a duplicate index, or a
  record outside the pre-state or the completed post-state.
