# libledger89 crash fixture table

Normative table for the crash suites. Each row fixes one persistence
boundary, the durable filesystem state after the crash, and the states
recovery may expose. The model filesystem (`test/support/model_fs.c`)
injects the crash; `mfs_crash` then discards every byte and name not made
durable.

## Model rules

```text
pwrite        modifies live bytes only
fsync         promotes live bytes to durable bytes
rename/unlink modifies the live namespace only
fsync_dir     promotes the whole live namespace (names and bytes) to durable
crash         live := durable (unsynced bytes and names vanish)
torn pwrite   persists a prefix of the bytes, then fails
```

A crash point is `(op, skip)`: crash on the `(skip+1)`-th call of `op`
during the operation. A torn crash persists half of the `pwrite` into the
page cache. "before" means the call had no effect; "during" persists a
prefix; "after" is the next call's "before".

Torn rows below show the page-cache state after the crash. On power loss
the torn prefix is itself unsynced and is discarded, so the durable state
is the pre-state; a process kill keeps it. The recovery oracle is the same
either way: only complete checksum-valid batches may become visible.

## Append (one batch; pre-state 1..3 durable)

| ID | Injection | Durable FS after crash | Allowed recovered state |
| --- | --- | --- | --- |
| C-A1 | crash before batch-header pwrite | 1..3 | 1..3 |
| C-A2 | crash during batch-header pwrite (torn) | 1..3 + partial header | 1..3 |
| C-A3 | crash before first record pwrite | 1..3 + header | 1..3 |
| C-A4 | crash during record header (torn) | 1..3 + partial record | 1..3 |
| C-A5 | crash during payload (torn) | 1..3 + partial payload | 1..3 |
| C-A6 | crash during record CRC (torn) | 1..3 + partial CRC | 1..3 |
| C-A7 | crash before second record (batch of 2) | 1..3 + complete first record | 1..3 |
| C-A8 | crash during final record | 1..3 + partial batch | 1..3 |
| C-A9 | crash before footer pwrite | 1..3 + records, no footer | 1..3 |
| C-A10 | crash during footer (torn) | 1..3 + partial footer | 1..3 |
| C-A11 | crash after batch bytes, before sync | 1..3 (live discarded) | 1..3 |
| C-A12 | crash during sync | 1..3 or 1..4 | 1..3 or 1..4 |
| C-A13 | crash after sync before API return | 1..4 | 1..4 |

Never allowed: a partial record or partial batch visible, a gap, or a
duplicate.

## Rotation (active 4..6, sealed 1..3; rotation seals 4..6)

| ID | Injection | Durable FS after crash | Allowed recovered state |
| --- | --- | --- | --- |
| C-R1 | crash before footer pwrite | active 4..6 | 1..6, one active |
| C-R2 | crash during footer pwrite (torn) | active + partial footer | 1..6, torn footer discarded |
| C-R3 | crash during footer fsync | active 4..6 or sealed 4..6 | 1..6 |
| C-R4 | crash before rename | active 4..6 | 1..6 |
| C-R5 | crash before rename's dir sync | either name | 1..6, one copy only |
| C-R6 | crash before new-active create | sealed 4..6, no active | 1..6, active recreated |
| C-R7 | crash during new-active header pwrite | sealed 4..6 + partial active | 1..6, active repaired |
| C-R8 | crash during new-active fsync | sealed 4..6, empty active | 1..6 |
| C-R9 | crash before final dir sync | sealed 4..6 + active name | 1..6 |

Never allowed: duplicate ranges, a gap, a missing record, or two active
segments.

## Truncation (pre-state 1..8 in segments; target N)

| ID | Injection | Durable FS after crash | Allowed recovered state |
| --- | --- | --- | --- |
| C-T1 | crash before active unlink | 1..8 | 1..k for k in [N,8] |
| C-T2 | crash between tail unlinks | 1..k for some k | 1..k, contiguous |
| C-T3 | crash during boundary rewrite (torn) | old or torn boundary | old 1..8 or post |
| C-T4 | crash before boundary rename | old boundary + no active | pre- or partial state |
| C-T5 | crash before fresh active header | truncated boundary, no active | 1..N or pre-state |
| C-T6 | crash during fresh active fsync | boundary + active header | 1..N |
| C-T7 | crash after completion | post-state | exactly 1..N |

Never allowed: a gap, a partial record, or an index above the pre-state
last.

## Discard (pre-state 1..8; target N)

| ID | Injection | Durable FS after crash | Allowed recovered state |
| --- | --- | --- | --- |
| C-D1 | crash before first front unlink | 1..8 | first in [1,N], last 8 |
| C-D2 | crash between front unlinks | suffix from a segment boundary | first in [1,N], last 8 |
| C-D3 | crash during boundary rewrite (torn) | old or torn boundary | old or post |
| C-D4 | crash before boundary rename | old boundary | pre- or partial |
| C-D5 | crash during active rewrite (discard into active) | old or new active | suffix intact |
| C-D6 | crash before empty active header (discard all) | old active | suffix intact |
| C-D7 | crash after completion | post-state | first == N, last 8 |

Never allowed: a gap, a removed retained record, or a resurrected index.

## Invariant checked at every row

After reopen: `first_index` and `last_index` are contiguous; every visible
index is readable; every visible record matches the pre-state pattern;
sealed ranges are contiguous with no duplicates; at most one active
segment.
