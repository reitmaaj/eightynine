# libledger89 design

## 1. Logical model

```text
base          index of the first record when the ledger is empty; 1 at creation
first_index   smallest present record index; equals base when empty
last_index    largest present record index; equals base - 1 when empty
```

Indices are caller-supplied and contiguous. The first record of an append
must equal `last_index + 1`; the following records increment by one. Index 0
is never a record index. `ledger89_append` accepts a batch and makes it
logically visible atomically: all records or none.

## 2. Physical model

Segments form a contiguous range chain:

```text
[first, last] [last+1, ...] ... [..., last]  active.seg
```

- Sealed segment files carry a unique creation key named `%020lu.seg`: the
  number is the segment's `first_index` at creation. Prefix discard rewrites
  a segment's header without changing its key, so the header's `first_index`
  is authoritative for ordering; recovery sorts by header range.
- `active.seg` is the only mutable file.
- Each append call is one physical batch delimited by a batch header and a
  checksummed batch footer.
- A sealed segment carries a header, batches, and a sealed footer with a
  whole-segment digest.

## 3. Module boundaries

| Module | Responsibility |
| --- | --- |
| `ledger89_crc.c` | CRC-32C incremental computation |
| `ledger89_format.c` | little-endian field codecs and structure encode/decode |
| `ledger89_internal.h` | internal types, I/O vtable, and handle state for fault injection |
| `ledger89_file.c` | POSIX implementation of the I/O vtable |
| `ledger89_segment.c` | segment table, append path, rotation, sealing |
| `ledger89_recover.c` | directory scan, topology validation, torn-tail recovery |
| `ledger89_truncate.c` | suffix truncation and prefix discard orderings |
| `ledger89_iter.c` | random read and ordered iteration |
| `ledger89.c` | public API glue, configuration, sync, observer, strerror |
| `ledger89_util.c` | pure helpers: bounds, index math, segment naming |

No module knows about Raft, queries, or application semantics. The public
header exposes only the section-3 surface of the specification.

## 4. Handle state

```text
io            I/O vtable + context
path          ledger directory
base          logical base index
first,last    visible range
segments      in-memory table of sealed segments {first,last,name}
active        active segment file handle, byte offset, record count
dirty         appends not yet fdatasync'ed
faulted       sticky I/O failure; mutations refused until reopen
observer      advisory callback + context
epoch         structural epoch; invalidates live iterators
lock_fd       flock on lock file; released on close
```

## 5. Error taxonomy

Positive values are states, negative values are errors, matching libraft89.

| Status | Meaning |
| --- | --- |
| `LEDGER89_OK` | success |
| `LEDGER89_END` | iteration finished |
| `LEDGER89_ERR_ARG` | invalid argument or logical misuse |
| `LEDGER89_ERR_IO` | I/O failure |
| `LEDGER89_ERR_NOMEM` | allocation failure |
| `LEDGER89_ERR_NOTFOUND` | index outside the visible range |
| `LEDGER89_ERR_RANGE` | value not representable or outside structural limits |
| `LEDGER89_ERR_SEQUENCE` | non-contiguous append index |
| `LEDGER89_ERR_CORRUPT` | structural corruption or checksum failure |
| `LEDGER89_ERR_BUSY` | another handle holds the ledger lock |
| `LEDGER89_ERR_FAULTED` | handle poisoned by an earlier I/O failure |
| `LEDGER89_ERR_STATE` | lifecycle/staleness violation |

Logical rejections (`ERR_ARG`, `ERR_SEQUENCE`, `ERR_RANGE`) are validated
before any byte is written. I/O failures during a mutation poison the handle;
`ledger89_close` and reopen recover by discarding the torn tail.

## 6. Ownership and lifetime

- `ledger89_record.data` is caller-owned and read only during `append`.
- `ledger89_view.data` from `read` is valid until the next call on the same
  handle; from `iter_next` until the next iterator call.
- The observer receives a view valid only for the duration of the callback;
  it must not call back into the ledger.
- One handle is caller-serialized; independent handles on independent
  ledgers may be used concurrently. `flock` yields `ERR_BUSY` for a second
  open of the same directory.
- `ledger89_close` releases the handle; live iterators must be closed first.

## 7. Limits

| Limit | Value |
| --- | --- |
| maximum record payload | 16 MiB (`LEDGER89_MAX_RECORD_BYTES`) |
| maximum batch record count | `UINT32_MAX` (on-disk field) |
| default `max_segment_bytes` (0) | 64 MiB |
| `max_segment_records` (0) | unlimited |
| public index/tag width | `unsigned long`; on-disk 64-bit little-endian |

A batch never splits across segments. If a single batch exceeds the segment
thresholds, it occupies a fresh segment by itself.
