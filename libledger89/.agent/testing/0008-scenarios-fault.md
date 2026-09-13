# BDD scenarios: I/O and allocation faults (L8)

Scenarios in this file drive `test/fault/test_fault_matrix.c`,
`test/fault/nomem_main.c`, and `test/fault/eintr_main.c`. IDs
cross-reference `.agent/acceptance/0008-fault.md`.

## I/O failure matrix

- SCENARIO Failure at any primitive: GIVEN one operation (append, rotate,
  truncate, discard) WHEN any single call of `pwrite`, `sync`, `rename`,
  `unlink`, `sync_dir`, `truncate`, `pread`, or `size` fails THEN the
  operation reports an error (or the failure is after its logical success),
  the handle faults for mutations, and a reopen exposes a legal contiguous
  state (F01).
- SCENARIO Usable after failure: GIVEN a failed operation WHEN the ledger is
  reopened THEN a subsequent append at `last_index + 1` and sync succeed
  (F02).
- SCENARIO ENOSPC during append: GIVEN a full disk simulated by a failing
  write WHEN append fails THEN the synced prefix is intact and, after space
  is freed and the ledger reopened, new appends succeed (F03).
- SCENARIO Read failure: GIVEN a failing `pread` WHEN `ledger89_read` or
  `ledger89_iter_next` runs THEN `LEDGER89_ERR_IO` and the ledger is not
  faulted (F04).
- SCENARIO Open failure: GIVEN a failing directory scan or segment read WHEN
  `ledger89_open` runs THEN `LEDGER89_ERR_IO`, `*out == NULL`, and a retry
  succeeds (F05).

## Allocation failure

- SCENARIO NOMEM at every site: GIVEN the (n+1)-th allocation fails WHEN any
  allocating operation runs THEN `LEDGER89_ERR_NOMEM`, no leak, and the
  logical ledger is unchanged or the handle is faulted (F06).
- SCENARIO Retry after NOMEM: GIVEN a NOMEM failure WHEN the same operation
  runs with allocation restored THEN it succeeds (F07).
- SCENARIO Allocation-free append: GIVEN any allocation failure WHEN
  `ledger89_append` runs THEN it succeeds without allocating (F08).

## EINTR

- SCENARIO EINTR on any syscall: GIVEN an injected `EINTR` from `pread`,
  `pwrite`, `fdatasync`, `fsync`, `ftruncate`, `rename`, or `unlink` WHEN
  the operation runs THEN the call is retried transparently, the operation
  succeeds, and the handle is never faulted (F09).
