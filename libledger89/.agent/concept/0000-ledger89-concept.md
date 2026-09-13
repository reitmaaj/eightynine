# libledger89 concept

`libledger89` is a small, standalone ISO C89 library for a **durable,
append-only sequence of opaque records** stored in immutable rotated segments,
with crash recovery and deterministic iteration. It is the local durability
core underneath higher layers; it is not an event store, database, WAL
framework, or Raft storage implementation.

## Position

```text
application
    |
    +-------- query/projection engine
    |               ^
    |               | ledger iteration
    |
    +-------- libraft89
    |               | append/truncate
    v               v
              libledger89
                    |
             segment files
```

## Responsibility boundary

| Concern | Owner |
| --- | --- |
| record encoding envelope | `libledger89` |
| monotonic local sequence | `libledger89` |
| append durability | `libledger89` |
| segment creation/sealing/rotation | `libledger89` |
| crash recovery | `libledger89` |
| checksums / corruption detection | `libledger89` |
| sequential/range iteration | `libledger89` |
| truncation of uncommitted tail | `libledger89`, when explicitly requested |
| prefix discard (retention) | `libledger89`, when explicitly requested |
| replication | `libraft89` |
| deciding commit | caller / `libraft89` integration |
| semantic event format | application |
| secondary indexes | external query engine |
| SQL/query language | external query engine |
| retention policy | caller |
| snapshots/projections | external component |

## Fundamental model

The ledger contains records identified by monotonically increasing indices
`1, 2, 3, ...`. A record carries an index, a `tag`, flags (reserved in v1),
an opaque payload, and a checksum. `libledger89` never interprets the payload.

The physical layout is a sequence of segments:

```text
ledger/
    00000000000000000001.seg
    00000000000001000001.seg
    00000000000002000001.seg
    active.seg
```

Sealed segments are immutable; only `active.seg` changes. Each sealed segment
covers a contiguous range `[first_index, last_index]`. Appends are atomic
multi-record batches: either every record of a batch becomes visible, or none
does.

## Appended versus committed

The library does not decide commitment and stores no commit index. It exposes:

- `ledger89_last_index` — the last logically visible record;
- `ledger89_sync` — the crash-durability barrier for preceding appends.

The caller remembers the semantic commit boundary. Suffix truncation exists
because replicated-log integration needs it; prefix discard exists for
retention and is never automatic.

## Invariants

1. Indices form a contiguous strictly increasing sequence.
2. Records never mutate after successful append except through suffix
   truncation.
3. Only the suffix may be truncated; only the prefix may be discarded.
4. Sealed segments never mutate.
5. Recovery exposes only complete checksum-valid batches.
6. A successful `ledger89_sync` establishes the crash-durability boundary.
7. Iteration always returns canonical append order.
8. Query/index state remains reconstructible from the ledger alone.
9. Replication semantics remain entirely external.
10. Record payloads remain opaque to `libledger89`.

## v1 scope

Included: caller-supplied contiguous indices; opaque records with a `tag`;
atomic batches; explicit `sync`; byte/record-count rotation; explicit rotation;
random read; bounded ordered iteration; crash recovery with torn-tail
discard; structural corruption classification; suffix truncation; prefix
discard; advisory observer; deterministic error taxonomy.

Excluded: replication, commit decisions, snapshots, query/index/SQL,
application schemas, wall-clock rotation, automatic retention, threads inside
the library.
