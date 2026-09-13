# Acceptance criteria: truncation and discard (L5)

These criteria trace to `.agent/testing/0005-scenarios-truncate-discard.md`
and are enforced by `test/api/test_truncate.c`, `test/api/test_discard.c`,
and `test/crash/test_truncate.c`.

## Must exhibit (exhibit)

- `ledger89_truncate_after(N)` MUST preserve exactly the records `<= N` and
  MUST remove every record `> N`, whether the boundary lies in the active
  segment, at a segment end, or inside a sealed segment.
- `ledger89_truncate_after(N)` with `N >= last_index` MUST be a no-op;
  `N == base - 1` MUST empty the ledger while preserving `base`;
  `N < base - 1` MUST return `LEDGER89_ERR_RANGE`.
- Appending after truncation MUST continue at `N + 1`, and formerly removed
  indices MUST carry only the new payloads after reopen.
- `ledger89_discard_before(N)` MUST preserve exactly the records `>= N`,
  advance the base to `N`, and clamp `N > last_index` to `last_index + 1`.
- Discarding all records MUST preserve the new base across reopen.
- Every crash point of both operations MUST recover a contiguous range: a
  prefix of the pre-state or a state where a prefix was discarded, never a
  gap, a partial record, or a resurrected index.
- Both operations MUST be self-durable on success.

## Must reject / fail safely (reject)

- Truncating below `base - 1` MUST return `LEDGER89_ERR_RANGE`.
- An I/O failure during either operation MUST fault the handle; a reopen
  MUST recover a legal contiguous state.
- Neither operation may expose a record that was removed, or remove a record
  that was retained.
- Iterators open across a structural mutation MUST report
  `LEDGER89_ERR_STATE`, not stale data.
