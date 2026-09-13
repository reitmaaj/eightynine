# Acceptance criteria: durability and clean reopen (L2)

These criteria trace to `.agent/testing/0002-scenarios-durability.md` and are
enforced by `test/api/test_sync.c` and `test/smoke.c`.

## Must exhibit (exhibit)

- `ledger89_append` MUST make a batch logically visible immediately: reads
  and iteration MUST return it before any `sync`.
- After a successful `ledger89_sync`, closing and reopening the ledger MUST
  return every preceding appended record byte-for-byte.
- A single `sync` after many appends MUST cover all of them.
- `ledger89_sync` MUST succeed and be idempotent on a clean or empty ledger.
- Appending after a reopen MUST continue the contiguous sequence at
  `last_index + 1`.

## Must reject / fail safely (reject)

- `ledger89_sync` on a faulted handle MUST return `LEDGER89_ERR_FAULTED` and
  MUST NOT claim durability.
- A failed `sync` MUST NOT advance the durable boundary claimed by the test
  oracle.
- `ledger89_sync(NULL)` MUST return `LEDGER89_ERR_ARG`.
