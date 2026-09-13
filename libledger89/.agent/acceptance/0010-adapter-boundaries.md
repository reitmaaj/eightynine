# Acceptance criteria: adapter boundaries (L10)

These criteria trace to `.agent/testing/0010-scenarios-adapter-boundaries.md`
and are enforced by `test/adapters/`. They exercise the public API only; no
Raft, replication, or application semantics live in the library.

## Must exhibit (exhibit)

- An adapter MUST be able to append contiguous batches, call `ledger89_sync`,
  close, reopen, and replay the exact sequence by iteration alone.
- After `ledger89_truncate_after` followed by fresh appends, iteration MUST
  yield a gap-free sequence whose suffix is exactly the fresh records and MUST
  NOT contain resurrected pre-truncation content.
- A projection fed by the advisory observer over successful appends MUST equal
  a projection rebuilt from iteration alone after reopen.
- An observer installed late MUST NOT prevent a correct full rebuild from
  iteration; the observer projection may be incomplete by construction.
- `ledger89_iter_open`/`ledger89_iter_next` MUST be sufficient to reconstruct
  every record index, tag, and payload byte in ascending order.

## Must reject / fail safely (reject)

- Appending an index `<= last_index` or `> last_index + 1` MUST return
  `LEDGER89_ERR_SEQUENCE` and MUST leave `first_index`, `last_index`, all
  records, and all on-disk bytes unchanged.
- `ledger89_truncate_after` with an index below `first_index - 1` MUST return
  `LEDGER89_ERR_RANGE` and MUST leave the visible range unchanged.
- A reversed explicit iteration range MUST return `LEDGER89_ERR_ARG` and MUST
  set `*out` to `NULL`.
- A rejected append MUST NOT call the observer; clearing the observer with
  `NULL` MUST stop all later callbacks.
