# Acceptance criteria: I/O and allocation faults (L8)

These criteria trace to `.agent/testing/0008-scenarios-fault.md` and are
enforced by `test/fault/test_fault_matrix.c`, `test/fault/nomem_main.c`, and
`test/fault/eintr_main.c`.

## Must exhibit (exhibit)

- An injected failure at any persistence primitive MUST NOT report success
  for a mutating operation unless the failure occurred after its logical
  success point.
- After a failed mutation, reopening MUST expose a legal contiguous state
  (pre-state, completed post-state, or a state permitted by the operation's
  crash contract) and the ledger MUST remain usable.
- A failing `pread` MUST yield `LEDGER89_ERR_IO` without faulting the handle
  for reads and iteration.
- A failed `ledger89_open` MUST leave `*out == NULL` and MUST NOT create or
  modify a partial ledger.
- An allocation failure at any site MUST return `LEDGER89_ERR_NOMEM`, MUST
  NOT leak memory, and MUST leave the logical ledger unchanged unless the
  operation had already reached an irreversible point, in which case the
  handle MUST be faulted and recoverable by reopen.
- An injected `EINTR` MUST be retried transparently for `pread`, `pwrite`,
  `fdatasync`, `fsync`, `ftruncate`, `rename`, and `unlink`; the operation
  MUST succeed and the handle MUST NOT fault.

## Must reject / fail safely (reject)

- No fault injection may produce a gap, a partial batch, a duplicated index,
  a corrupted sealed segment, or a silently skipped record.
- A failed operation MUST NOT permanently wedge the ledger: after the fault
  is cleared and the ledger reopened, appends and syncs MUST succeed.
- A NOMEM during a structural operation after irreversible steps MUST fault
  the handle rather than continue with stale in-memory state.
