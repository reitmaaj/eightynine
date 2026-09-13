# Acceptance criteria: core API (L1)

These criteria trace to `.agent/testing/0001-scenarios-core-api.md` and are
enforced by `test/smoke.c` and `test/api/`.

## Must exhibit (exhibit)

- `ledger89_open` MUST create a missing ledger directory and MUST report an
  empty ledger as `first_index == 1`, `last_index == 0`.
- Reopening a closed ledger MUST return the exact record sequence with exact
  tags and payload bytes.
- `ledger89_append` MUST accept a contiguous batch whose first index equals
  `last_index + 1` and MUST make all records visible atomically on success.
- `ledger89_read` MUST return the exact record for every index in
  `[first_index, last_index]`.
- `ledger89_iter_open`/`ledger89_iter_next` MUST return records in strictly
  ascending index order without duplication or omission and MUST end with
  `LEDGER89_END`.
- Zero index bounds MUST mean "from the first record" / "to the last record";
  out-of-range bounds MUST clamp; an empty ledger MUST yield an iterator that
  ends immediately.
- The advisory observer MUST receive each appended record once in ascending
  order after the batch becomes visible.

## Must reject / fail safely (reject)

- A null output pointer, null config, or null path MUST return
  `LEDGER89_ERR_ARG` and set `*out` to `NULL`.
- A second simultaneous open of the same ledger MUST return
  `LEDGER89_ERR_BUSY`.
- A batch whose first index is not `last_index + 1`, or that contains a gap
  or duplicate, MUST return `LEDGER89_ERR_SEQUENCE` and MUST leave
  `last_index`, all existing records, and all on-disk bytes unchanged.
- A zero-count batch, a null record array with positive count, a null data
  pointer with positive size, or a payload above 16 MiB MUST return
  `LEDGER89_ERR_ARG` and MUST leave the ledger unchanged.
- Reading or iterating outside the visible range MUST return
  `LEDGER89_ERR_NOTFOUND` or an empty iteration; no out-of-range read may
  return another record's data.
- A rejected append MUST NOT call the observer.
