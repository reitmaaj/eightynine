# libledger89 — specification

Normative contract for libledger89 v1. Sections map to tests under `test/`
and scenarios under `.agent/testing/`. The public surface is
`include/ledger89.h`; byte layouts are in `.agent/design/0001-format-v1.md`,
durability semantics in `.agent/design/0002-durability-contract.md`.

## 1. Purpose

libledger89 owns a local, durable, append-only sequence of opaque records
stored in immutable rotated segments, with crash recovery and deterministic
iteration. It owns nothing else: no replication, commit decisions, query
semantics, indexes, SQL, snapshots, or application schemas. Record payloads
are opaque.

## 2. Scope

Included: caller-supplied contiguous indices; opaque records with a `tag`;
atomic append batches; explicit `sync`; byte/record-count and explicit
rotation; random read; bounded ordered iteration; crash recovery with
torn-tail discard; structural corruption detection; suffix truncation; prefix
discard; advisory observer.

Excluded: replication, commit decisions, snapshots, query/index/SQL,
application schemas, wall-clock rotation, automatic retention, threads.

## 3. Public API

```c
int  ledger89_open(ledger89 **out, const ledger89_config *config);
void ledger89_close(ledger89 *l);
int  ledger89_append(ledger89 *l, const ledger89_record *records, size_t count);
int  ledger89_sync(ledger89 *l);
int  ledger89_rotate(ledger89 *l);
int  ledger89_truncate_after(ledger89 *l, ledger89_index index);
int  ledger89_discard_before(ledger89 *l, ledger89_index index);
int  ledger89_set_observer(ledger89 *l, ledger89_observer_fn fn, void *ctx);
ledger89_index ledger89_first_index(const ledger89 *l);
ledger89_index ledger89_last_index(const ledger89 *l);
int  ledger89_read(ledger89 *l, ledger89_index index, ledger89_view *out);
int  ledger89_iter_open(ledger89 *l, ledger89_index first, ledger89_index last,
                        ledger89_iter **out);
int  ledger89_iter_next(ledger89_iter *it, ledger89_view *out);
void ledger89_iter_close(ledger89_iter *it);
const char *ledger89_strerror(int status);
```

`ledger89_close(NULL)` is a no-op. `ledger89_iter_close(NULL)` is a no-op.

## 4. Index model

Indices are `unsigned long`, start at 1, and are caller-supplied. An empty
ledger has `first_index == base` and `last_index == base - 1`; a freshly
created ledger has `base == 1`. `ledger89_append` requires the first record's
index to equal `last_index + 1`; each following record increments by one.
Index 0 is never a record index.

## 5. Open, create, close

`ledger89_open` creates the directory if missing, acquires an exclusive
advisory lock (`LEDGER89_ERR_BUSY` on conflict), validates the segment
topology, recovers the active segment, and exposes the visible range. It
validates structure only: segment headers, sealed footers, and name/range
ordering. Record CRCs are validated on access. `*out` is `NULL` on every
failure path. The caller must close live iterators before closing the handle.

## 6. Append

`ledger89_append` pre-validates the entire batch before writing:

- `l != NULL`, `records != NULL` when `count > 0`, `count > 0`,
  `count <= UINT32_MAX`;
- every `data != NULL` when `size > 0`, and `size <= 16 MiB`;
- the first index equals `last_index + 1` and indices are consecutive.

Logical rejections return `LEDGER89_ERR_ARG` or `LEDGER89_ERR_SEQUENCE` with
no state change and no bytes written. On success every record of the batch is
logically visible and the observer (if set) is called once per record in
ascending order. Durability requires a later successful `ledger89_sync`.

## 7. Sync

`ledger89_sync` fdatasyncs the active segment when dirty and returns
`LEDGER89_OK` otherwise. After success, every preceding successful append is
recoverable after process crash, OS crash, or power loss, subject to the
filesystem honoring the required primitives.

## 8. Rotation

Rotation is triggered before an append when the active segment is non-empty
and adding the batch would exceed `max_segment_bytes` or
`max_segment_records`. A batch never splits across segments; a batch larger
than the thresholds occupies a fresh segment by itself. `ledger89_rotate`
seals the current active segment; rotating an empty active segment is a
no-op. Rotation is self-durable and preserves the exact logical sequence.
Sealed segments never change afterward.

## 9. Read and iteration

`ledger89_read` returns the exact record at an index in
`[first_index, last_index]`; any other index returns
`LEDGER89_ERR_NOTFOUND`. The returned view is valid until the next call on
the same handle.

`ledger89_iter_open` takes inclusive bounds. `first == 0` means "from the
first record" and `last == 0` means "to the last record"; nonzero bounds are
clamped into range; an explicit reversed range (`first > last`, both nonzero)
returns `LEDGER89_ERR_ARG`. Iteration is always ascending by index and ends
with `LEDGER89_END`. Each view is valid until the next iterator call.
Iterators detect structural mutation through the handle epoch and return
`LEDGER89_ERR_STATE` when stale.

## 10. Suffix truncation

`ledger89_truncate_after(index)` keeps exactly the records `<= index` and
removes every record `> index`. `index >= last_index` is a no-op;
`index == base - 1` empties the ledger while preserving `base`;
`index < base - 1` returns `LEDGER89_ERR_RANGE`. Truncation is self-durable,
never creates a gap, and never resurrects a discarded index. Appending after
truncation continues at `index + 1` with fresh content.

## 11. Prefix discard

`ledger89_discard_before(index)` keeps exactly the records `>= index` and
removes every record `< index`. `index <= first_index` is a no-op;
`index > last_index` clamps to `last_index + 1`; the new base becomes the
clamped index. Discard is self-durable, never creates a gap, and never
removes a retained record. It is never automatic.

## 12. Observer

`ledger89_set_observer` installs an advisory callback called once per record
of a successful append, in ascending order, after logical visibility and
before `sync`. It is never called on a rejected append or during recovery.
The callback must not call back into the ledger. It is advisory only: a
query engine must be able to rebuild from iteration alone.

## 13. Error taxonomy

Positive values are states, negative values are errors:
`LEDGER89_OK`, `LEDGER89_END`, `LEDGER89_ERR_ARG`, `LEDGER89_ERR_IO`,
`LEDGER89_ERR_NOMEM`, `LEDGER89_ERR_NOTFOUND`, `LEDGER89_ERR_RANGE`,
`LEDGER89_ERR_SEQUENCE`, `LEDGER89_ERR_CORRUPT`, `LEDGER89_ERR_BUSY`,
`LEDGER89_ERR_FAULTED`, `LEDGER89_ERR_STATE`.

## 14. Durability and recovery

The contract in `.agent/design/0002-durability-contract.md` is normative:
append batches are atomic across crashes; sync is the durable boundary;
torn tails are discarded; corruption before the recoverable tail is
`LEDGER89_ERR_CORRUPT` and is never skipped; sealed segments are immutable;
rotation, truncation, and discard are self-durable and preserve contiguity.
An I/O failure during a mutation poisons the handle with
`LEDGER89_ERR_FAULTED`; close and reopen recovers. `EINTR` is retried.

## 15. Limits

Maximum record payload 16 MiB. Maximum batch record count `UINT32_MAX`.
Default `max_segment_bytes` 64 MiB when zero. `max_segment_records` zero
means unlimited. On-disk indices and tags are 64-bit; values not
representable in `unsigned long` on the host return `LEDGER89_ERR_RANGE`.
