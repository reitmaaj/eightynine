# libledger89 Raft adapter (design)

Replication and consensus sit above or beside storage. The ledger is never
configured with a Raft plugin; an adapter composes the two.

```text
             application
                  |
          replicated-ledger
               adapter
              /       \
             v         v
       libraft89   libledger89
```

## 1. Record envelope

The ledger stores opaque bytes. The adapter encodes each Raft entry as:

```text
u64 term (little-endian) ‖ command bytes
```

`log_term` reads the first 8 bytes; `log_size` returns `size - 8`;
`log_read` returns the command bytes.

## 2. Store callbacks

| `raft89_store` | ledger mapping |
| --- | --- |
| `hard_state` | adapter-owned atomic file (not the ledger) |
| `log_last` | `end - 1` (0 when `end == 1`) |
| `log_term` | `ledger89_read(end - 1)` + envelope decode |
| `log_size` | `ledger89_read` size minus envelope |
| `log_read` | `ledger89_read` payload slice |

## 3. Actions

| `raft89_action` | adapter handling |
| --- | --- |
| `RAFT89_ACT_HARD_STATE` | write tmp, fsync, rename, dir fsync |
| `RAFT89_ACT_LOG_APPEND` | `appendv_at(rev, entries[0].index - 1)` then `sync`; `ESTALE` -> `RAFT89_ACTION_FATAL` |
| `RAFT89_ACT_LOG_TRUNCATE` | sync if dirty, then `truncate_from(first_index)` |
| `RAFT89_ACT_APPLY` | deliver to the application state machine; persist applied state outside the ledger |
| `RAFT89_ACT_SEND` | transport outside both libraries |

## 4. Boundary rule

`ledger89` `stable_end` means locally durable. `libraft89` `commit_index`
means replicated consensus commit. The adapter never merges them; it maps
storage actions to ledger operations and reports completion back to Raft.

## 5. Compaction

`prune_before` may run only after the application has durably captured a
snapshot that covers the pruned prefix. Snapshot policy and format remain
outside the ledger.

## 6. Crash argument

Hard state is persisted atomically before any dependent log append is
acknowledged. Log appends are compare-and-appended and synced before
`RAFT89_ACTION_OK`. Truncation requires a clean ledger and is durable before
return. After restart, Raft re-reads hard state and log metadata and
recomputes volatile commit/applied state; the ledger's stable frontier bounds
which log entries can exist.

## 7. Fixture

`test/adapters/raft_adapter_main.c` is a compiling adapter fixture built by
`just adapters-raft` against the sibling `libraft89` tree (SKIPPED when the
sibling is absent). It maps `RAFT89_ACT_HARD_STATE`, `LOG_APPEND`,
`LOG_TRUNCATE`, and `APPLY` onto a ledger, proposes a command on a
single-node cluster, and verifies that the committed entry is stored with a
term envelope and a durable stable frontier. The WAL-style,
replicated-log-style, and checkpoint consumers in `test/adapters/test_*.c`
cover the remaining boundary behavior.
