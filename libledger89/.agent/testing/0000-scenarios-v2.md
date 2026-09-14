# BDD scenarios: libledger89 v2

Driven by `test/smoke.c`, `test/unit/`, `test/api/`, `test/adapters/`,
`test/crash/`, `test/fault/`, `test/corrupt/`, `test/model/`, `test/golden/`,
and `test/stress/`. Acceptance criteria are in
`.agent/acceptance/0000-acceptance-v2.md`.

## Identity and state

SCENARIO O01 create
  GIVEN a fresh directory
  WHEN the ledger is opened RDWR|CREATE|EXCL
  THEN first == stable_end == end == 1 and revision == 0

SCENARIO O02 identity stability
  GIVEN a created ledger
  WHEN it is closed and reopened
  THEN the 16-byte identity is unchanged

SCENARIO O03 ownership
  GIVEN an open writable handle
  WHEN a second writable or read-only open is attempted
  THEN EBUSY is returned and no handle is produced

SCENARIO O04 read-only
  GIVEN a read-only open
  WHEN appendv, sync, truncate_from, prune_before, or rotate is called
  THEN EROFS is returned

SCENARIO O05 flags
  GIVEN invalid flag combinations
  WHEN open is called
  THEN EINVAL is returned

## Append and durability

SCENARIO A01 ledger-assigned positions
  GIVEN end == 42
  WHEN appendv supplies three slices
  THEN first_out == 42 and the new end is 45

SCENARIO A02 batch atomicity
  GIVEN a valid batch
  WHEN appendv fails logically
  THEN no record is visible and end is unchanged

SCENARIO A03 compare-and-append
  GIVEN revision R and end E
  WHEN appendv_at is called with different values
  THEN ESTALE is returned with no change
  WHEN appendv_at matches both
  THEN the batch is appended

SCENARIO A04 sync frontier
  GIVEN appended but unsynced records
  WHEN sync succeeds
  THEN stable_end == end
  AND a crash discards the previously unsynced tail

SCENARIO A05 failed sync
  GIVEN an injected I/O failure during sync
  WHEN a later mutation is attempted
  THEN EPOISONED is returned until reopen

## Read and iteration

SCENARIO R01 bounds
  GIVEN records in [first, end)
  WHEN read is called with index < first, index >= end, a short buffer, or a
       NULL buffer
  THEN EGONE, ENOENT, ETOOSMALL (size reported), and size-only OK follow

SCENARIO R02 iterator invalidation
  GIVEN an open iterator
  WHEN truncate_from succeeds
  THEN the next call returns ESTALE
  WHEN pruning passes the iterator position
  THEN EGONE is returned
  AND ordinary append/sync never invalidate

SCENARIO R03 DONE is not permanent
  GIVEN an iterator positioned at end
  WHEN appendv extends the ledger
  THEN the next call returns the new record

## Structural operations

SCENARIO S01 truncate requires clean
  GIVEN end > stable_end
  WHEN truncate_from is called
  THEN EUNSTABLE is returned

SCENARIO S02 truncation
  GIVEN a clean ledger
  WHEN truncate_from(N) succeeds
  THEN end == stable_end == N, revision increments, and the suffix is gone
  AND indices at or above N may be reused

SCENARIO S03 pruning granularity
  GIVEN sealed parts
  WHEN prune_before is called
  THEN only whole sealed parts are removed, actual_first <= requested, and
       revision is unchanged

SCENARIO S04 rotation
  GIVEN a non-empty active part
  WHEN rotate succeeds
  THEN the active part is sealed and a new empty active part starts
  AND rotating an empty active part is a no-op

## Recovery

SCENARIO C01 unsynced tail
  GIVEN durable records and a complete unsynced batch
  WHEN power loss and reopen occur
  THEN only the stable prefix is recovered

SCENARIO C02 torn marker
  GIVEN a valid marker followed by a torn marker
  WHEN recovery runs
  THEN the previous marker is the frontier

SCENARIO C03 stable corruption
  GIVEN framing corruption before the chosen marker
  WHEN open runs
  THEN ECORRUPT is returned and nothing is repaired

SCENARIO C04 topology publication
  GIVEN a structural operation
  WHEN a crash occurs at any syscall
  THEN the recovered ledger is the old or the new topology, never a mixture

SCENARIO C05 manifest selection
  GIVEN CURRENT naming a corrupt or missing manifest
  WHEN open runs
  THEN ECORRUPT is returned and no older manifest is used
  AND a greater-generation orphan manifest is ignored

SCENARIO C06 real process kills
  GIVEN a child process killed at an armed syscall
  WHEN the parent reopens
  THEN the recovered state is one of the permitted states and stays usable

## Faults, corruption, model, adapters

SCENARIO F01 fault matrix
  GIVEN a single injected I/O failure in a mutation
  WHEN the handle is poisoned and reopened
  THEN a legal state is recovered and the ledger remains usable

SCENARIO X01 corruption sweep
  GIVEN byte flips in CURRENT, manifests, part headers, markers, and batch
       framing
  WHEN open/read run
  THEN ECORRUPT or EFORMAT is returned, never silent repair

SCENARIO M01 randomized model
  GIVEN a random operation sequence
  WHEN compared against a reference model
  THEN first/stable_end/end/revision and all payloads match exactly,
       including after simulated power loss

SCENARIO AB01 adapter boundaries
  GIVEN WAL-style, replicated-log-style, and checkpoint consumers
  WHEN they replay, replace suffixes, and detect stale revisions or pruned
       positions
  THEN the ledger exposes exactly the required primitives

SCENARIO G01 golden fixtures
  GIVEN frozen byte fixtures
  WHEN the same ledger is rebuilt or decoded
  THEN the bytes match and a flipped payload byte fails the read
