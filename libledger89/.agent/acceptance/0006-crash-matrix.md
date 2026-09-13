# Acceptance criteria: crash matrix (L6)

These criteria trace to `.agent/testing/0006-crash-fixtures.md` and are
enforced by `test/crash/test_crash_append.c`,
`test/crash/test_crash_rotate.c`, `test/crash/test_crash_truncate.c`, and
`test/crash/test_real_kill.c`.

## Must exhibit (exhibit)

- For every append crash point, recovery MUST expose either the pre-state or
  the pre-state plus the complete batch; a torn record or batch MUST never
  be visible.
- For every rotation crash point, recovery MUST expose the exact logical
  sequence, exactly one active segment, and contiguous sealed ranges.
- For every truncation crash point, recovery MUST expose a contiguous prefix
  `1..k` with `k` between the target and the pre-state last.
- For every discard crash point, recovery MUST expose a contiguous suffix
  `m..last` with `m` at most the target.
- After every crash, every visible record MUST match the pre-state content
  byte-for-byte, and every visible index MUST be readable.
- The real-filesystem process-kill suite MUST recover a legal state after
  `_exit` at append, rotation, and truncation boundaries.

## Must reject / fail safely (reject)

- No crash point may expose a partial batch, a gap, a duplicate index, two
  active segments, overlapping sealed ranges, or a record outside the
  pre-state or the completed post-state.
- A crash must never turn a valid pre-state into `LEDGER89_ERR_CORRUPT`.
- The recovered ledger must be immediately usable: a subsequent append at
  `last_index + 1` and sync MUST succeed.
