# libledger89 concept (v2)

`libledger89` is a small, standalone ISO C89 library for a **locally durable,
ordered, rewindable sequence of opaque byte records, addressed by stable
logical positions**. It is the neutral substrate underneath WALs, message
queues, IPC logs, queryable event stores, and replicated logs; it is not any
of those.

## Position

```text
                    libwal89
                    libmq89
                libipcledger89
                   libksql89
                       |
                       v
                 libledger89
          ordered durable byte records
                       ^
                       |
             storage adapter/composition
                       |
                   libraft89
```

Replication and consensus sit above or beside storage; an adapter composes
`libraft89` and `libledger89`. The ledger never calls into Raft.

## Fundamental model

```text
[first, stable_end)   locally durable records
[stable_end, end)     appended but not yet durable
```

A record is `{index, payload}`. The physical representation adds framing,
lengths, and checksums. The library assigns contiguous indices; a freshly
created ledger has `first == stable_end == end == 1`.

`stable` means exactly one thing: the local storage contract guarantees
recovery of this prefix after a successful `sync`. It never means consensus
commit, message acknowledgement, or transaction commit.

## Structural operations

```text
appendv/appendv_at   extend the right edge
sync                 advance the stable edge
truncate_from        move the right edge left (revision increments)
prune_before         move the left edge right (revision unchanged)
rotate               seal the active part
```

## Invariants

1. `first <= stable_end <= end`.
2. Indices are contiguous within `[first, end)` and assigned by the ledger.
3. A batch is atomic: all records or none.
4. A successful sync makes `stable_end == end` and is the exact recovery
   frontier.
5. Sealed parts and published manifests are never destructively modified.
6. Every crash resolves to one published topology plus an exact stable prefix.
7. Corruption in stable history is `ECORRUPT`, never silently truncated.
8. Truncation increments `revision`; pruning does not.
9. Payloads remain opaque to `libledger89`.

## Excluded

Transactions, WAL/MQ semantics, topics, acks, subscriptions, consumer groups,
SQL/query semantics, schemas, secondary indexes, replication, consensus,
membership, networking, threads, IPC, snapshots, retention policy, timestamps,
application record types, serialization, and wall-clock behavior.
