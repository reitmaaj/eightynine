# libledger89 — specification (v2)

Normative contract for libledger89 v2. Sections map to tests under `test/`
and scenarios under `.agent/testing/`. The public surface is
`include/ledger89.h`; the byte layouts are in
`.agent/design/0001-format-v2.md`; durability and recovery semantics are in
`.agent/design/0002-durability-contract.md`.

## 1. Purpose

libledger89 owns a local, durable, ordered, rewindable sequence of opaque
byte records addressed by stable logical positions. It is not a WAL, message
log, or Raft log; those are interpretations supplied by higher layers. The
library never interprets payload bytes.

## 2. Logical model

```text
[first, stable_end)   locally durable records
[stable_end, end)     appended but not yet durable
```

A freshly created ledger has `first == stable_end == end == 1`; position 0 is
never a record position. `first <= stable_end <= end` always holds. A
successful `sync` makes `stable_end == end`.

## 3. Public API

```c
int ledger89_open(ledger89 **out, const char *path, unsigned long flags);
void ledger89_close(ledger89 *l);
int ledger89_get_state(ledger89 *l, ledger89_state *state_out);
int ledger89_appendv(ledger89 *l, const ledger89_slice *records, size_t count,
                     ledger89_index *first_out);
int ledger89_appendv_at(ledger89 *l, ledger89_revision expected_revision,
                        ledger89_index expected_end,
                        const ledger89_slice *records, size_t count,
                        ledger89_index *first_out);
int ledger89_append(ledger89 *l, const void *data, size_t size,
                    ledger89_index *index_out);
int ledger89_sync(ledger89 *l, ledger89_index *stable_end_out);
int ledger89_read(ledger89 *l, ledger89_index index, void *data_out,
                  size_t capacity, size_t *size_out);
int ledger89_read_at(ledger89 *l, ledger89_index index, size_t offset,
                     void *data_out, size_t size);
int ledger89_iter_init(ledger89_iter *it, ledger89 *l, ledger89_index from);
int ledger89_iter_next(ledger89_iter *it, ledger89_index *index_out,
                       void *data_out, size_t capacity, size_t *size_out);
int ledger89_truncate_from(ledger89 *l, ledger89_index from);
int ledger89_prune_before(ledger89 *l, ledger89_index requested_first,
                          ledger89_index *actual_first_out);
int ledger89_rotate(ledger89 *l);
int ledger89_verify(ledger89 *l);
const char *ledger89_strerror(int result);
int ledger89_u64_cmp(ledger89_u64 a, ledger89_u64 b);
int ledger89_u64_equal(ledger89_u64 a, ledger89_u64 b);
ledger89_u64 ledger89_u64_zero(void);
ledger89_u64 ledger89_u64_from_u32(ledger89_u32 value);
```

The library assigns contiguous indices; callers cannot choose positions.

## 4. Open, create, close

Exactly one of `RDONLY`/`RDWR` is required; `CREATE` requires `RDWR`; `EXCL`
requires `CREATE`. A writable open takes exclusive ownership (`EBUSY` on
conflict) and physically discards the unstable tail; a read-only open takes
shared ownership, never mutates, and returns `EROFS` for mutations. Open
performs recovery before returning. Corruption before the last durable
frontier returns `ECORRUPT`; unsupported versions return `EFORMAT`. On every
failure path `*out` is NULL.

## 5. Append

`appendv` assigns consecutive indices starting at the current `end`. count
must exceed zero; each slice must satisfy `size > 0 => data != NULL`; a
payload above `LEDGER89_MAX_RECORD_BYTES` returns `ERANGE`; a count above
`UINT32_MAX` returns `ERANGE`; `end + count` overflow returns `EOVERFLOW`.
The records become visible immediately and durable after `sync`. An append
failure never exposes a partial logical batch.

`appendv_at` performs the same append only when the current revision and end
match the caller's expectation; otherwise it returns `ESTALE` with no state
change. This is compare-and-append, not caller-assigned positions.

## 6. Sync

`sync` writes an explicit stable marker for the current `end` and flushes it.
On success `stable_end == end`; on a clean ledger it is a no-op. A failure for
which the durable frontier cannot be determined poisons the handle:
subsequent mutations return `EPOISONED` and reopen discovers the actual
state. A failed sync may still have reached storage; recovery accepts the new
frontier only if its prefix validates.

## 7. Read and iteration

`read` returns `EGONE` for `index < first`, `ENOENT` for `index >= end`,
`ETOOSMALL` (with the required size) when the buffer is short, and supports a
NULL buffer for size-only queries. Reading `[stable_end, end)` is permitted;
those records may disappear after crash/reopen.

`read_at` copies exactly `size` bytes beginning at `offset` within one record.
`size == 0` permits a NULL buffer and succeeds whenever `offset <= record
size`. `offset` past the record end, or a range crossing the record end,
returns `ERANGE` and copies nothing. A non-zero copy verifies the complete
record checksum before reporting success, so corruption outside the requested
range is still reported as `ECORRUPT`. `read_at` never mutates ledger state.

`iter_init` returns `EGONE` for `from < first`, `ERANGE` for `from > end`,
and accepts `from == end`. `iter_next` precedence is `EGONE`, `ESTALE`,
`DONE`; `DONE` is not permanent. `append`, `sync`, and pruning that does not
pass the iterator do not invalidate it; `truncate_from` always does.

## 8. Structural operations

`truncate_from(from)` requires a clean ledger (`EUNSTABLE` otherwise), keeps
`[first, from)`, sets `end = stable_end = from`, and increments `revision`
unless `from == end`. It is copy-on-write and durable before returning.

`prune_before(requested)` requires `requested <= stable_end` (`EUNSTABLE`
otherwise), removes only whole sealed parts whose end is at or below the
request, and reports the actual first retained position. `revision` does not
change. It may run while the active tail is dirty.

`rotate` seals the active part and starts a new one; rotating an empty active
part is a no-op.

`verify` recomputes sealed digests and batch checksums, returning `OK`,
`ECORRUPT`, or an I/O error.

## 9. Errors

`LEDGER89_OK`, `LEDGER89_DONE`, `EINVAL`, `ENOMEM`, `EIO`, `ENOENT`,
`EEXIST`, `EBUSY`, `EROFS`, `ECORRUPT`, `EFORMAT`, `ESTALE`, `EGONE`,
`ERANGE`, `EUNSTABLE`, `ETOOSMALL`, `EOVERFLOW`, `EPOISONED`.

## 10. Limits

Maximum record payload 16 MiB. Maximum batch record count `UINT32_MAX`.
Automatic rotation target 64 MiB (internal; explicit `rotate` is always
available). Indices and revisions are 64-bit.
