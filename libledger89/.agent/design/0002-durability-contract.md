# libledger89 durability contract

This document is normative for the library's durability guarantees and for
the test oracle that validates them.

## 1. Durability states

```text
logical   records visible through the public API in this process
durable   records recoverable after process crash, OS crash, or power loss
```

`ledger89_append` makes a batch logically visible on success. `ledger89_sync`
establishes the durable boundary: after a successful sync, every preceding
successful append and every completed structural operation remains
recoverable, subject to the filesystem honoring the required primitives.

## 2. Durability primitives

| Operation | File sync | Directory sync |
| --- | --- | --- |
| append | deferred to `sync` (`fdatasync`) | no |
| `sync` | `fdatasync(active)` when dirty | no |
| rotate | footer `fsync`, new active header `fsync` | after rename, after create |
| truncate/discard | rewrite `fsync` | after every unlink and rename |

`rotate`, `truncate_after`, and `discard_before` are **self-durable**: each
step is individually crash-safe and synced before the next, and the operation
returns only when its result is durable. `sync` is the barrier for appends.

## 3. Crash-state rule

For any operation and any crash point, the recovered ledger must be one of
the states explicitly permitted for that operation. No crash may expose:

- a partial batch (some records of a batch but not all);
- a gap or duplicate in the index sequence;
- a mutated sealed segment;
- a record not present in the pre-state or in the completed post-state;
- silently skipped damaged history.

## 4. Append

Append pre-validates the whole batch before writing. On success the batch is
logically visible. A crash before `sync` may recover the pre-state or the
complete batch, never a prefix of it: the batch footer CRC distinguishes a
complete batch from a torn one.

## 5. Torn tail versus corruption

Recovery parses the active segment batch by batch. At the first invalid
position `O`:

- if a fully valid batch exists later in the file, the damage is inside
  history: `LEDGER89_ERR_CORRUPT`;
- otherwise the bytes from `O` to end of file are a torn tail: they are
  truncated and recovery continues.

The region after the last complete valid batch is always the recoverable
tail; record CRC failures there are treated as torn, not as corruption.

## 6. Faulted handles

Any I/O failure during a mutating operation marks the handle faulted.
Subsequent mutations return `LEDGER89_ERR_FAULTED`; reads may still be
attempted. The caller frees space or repairs the environment, then closes and
reopens: recovery truncates any torn tail and the ledger is usable again.
`EINTR` is retried transparently and never faults the handle.

## 7. Rotation

```text
flush active
write sealed footer
fsync file
rename active.seg -> NNNN.seg
fsync directory
create new active.seg (header)
fsync file
fsync directory
```

Every prefix of these steps is recoverable: old active, or sealed segment
with or without a new active, or sealed segment plus new active. Recovery
creates a missing active and treats an empty active with a stale
`first_index` as canonical `last sealed + 1`.

## 8. Truncation and discard

Tail segments are removed from the end backward, one unlink plus directory
fsync at a time; the boundary segment is rewritten atomically through a
`.tmp` file, fsync, rename, and directory fsync. Prefix discard removes
wholly discarded segments from the front the same way, then rewrites the
boundary segment under its new `first_index` (sealed segments are renamed to
the new first index). Discard-all keeps the last segment as the base anchor
until the new empty active exists, then removes it. Every intermediate
durable state is a contiguous range.

## 9. Named crash properties

```text
CR01 no partial batch is ever visible after recovery
CR02 a successful sync makes all preceding appends durable
CR03 recovery never skips corruption before the recoverable tail
CR04 sealed segment bytes never change after sealing
CR05 rotation preserves the exact logical sequence
CR06 truncation never creates a gap and never resurrects a discarded index
CR07 discard never creates a gap and never removes a retained index
CR08 repeated recovery is idempotent
CR09 a faulted handle never reports success for a mutation
CR10 on-disk parsing never trusts unchecked lengths, offsets, or counts
```

Each property maps to tests under `test/crash/` and to rows in
`.agent/testing/0005-crash-fixtures.md`.
