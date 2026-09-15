# libledger89 design (v2)

## 1. Logical model

```text
first        oldest retained record position
stable_end   end of the locally durable prefix
end          next append position
revision     identity of the current index mapping
uuid         persistent ledger identity
```

`first <= stable_end <= end`. `appendv` changes only `end`; `sync` changes
only `stable_end`; `prune_before` changes only `first`; `truncate_from`
changes `end`, `stable_end`, and `revision`. A fresh ledger starts at 1.

## 2. Physical model

```text
ledger/
    CURRENT                    atomically replaced pointer
    MANIFEST.<16hex-gen>       immutable published topology
    part.<16hex-file_id>       active or sealed, per manifest
    CURRENT.tmp                transient publish staging
    lock                       advisory ownership
```

Uniform part names make sealing non-destructive: appending a sealed footer
to the active part is invisible to the previous topology (recovery ignores
bytes after the last marker), so no referenced file is ever renamed or
rewritten. `CURRENT` alone selects the authoritative manifest; a manifest
with a greater generation is an orphan.

## 3. Module boundaries

| Module | Responsibility |
| --- | --- |
| `ledger89_u64.c` | portable scalar helpers and internal 64-bit arithmetic |
| `ledger89_crc.c` | CRC-32C |
| `ledger89_format.c` | CURRENT/manifest/part/batch/marker/footer codecs |
| `ledger89_file.c` | POSIX I/O vtable, including `entropy` |
| `ledger89_manifest.c` | CURRENT and manifest publication, orphan GC |
| `ledger89_segment.c` | part creation/scanning, batches, markers, sealing, rotation |
| `ledger89_recover.c` | open-time recovery, marker scan, forward validation |
| `ledger89_read.c` | batch directory lookup, random read, exact byte-range read |
| `ledger89_truncate.c` | copy-on-write truncation and pruning |
| `ledger89_util.c` | pure name/path helpers, buffer growth, state guards |
| `ledger89.c` | public API glue, state, poisoning, `strerror` |

## 4. Handle state

```text
io                I/O vtable + context
path/scratch      ledger directory and path buffers
writable          RDWR vs RDONLY
poisoned          sticky uncertain-durability failure
dirty             end > stable_end
id/revision       identity and index-mapping generation
first/stable/end  three-boundary model
generation        current manifest generation
parts[]           manifest-ordered parts; active last
dir[]             in-memory batch directory for random access
next_file_id      next copy-on-write part id
lock_fd           advisory ownership
```

## 5. Runtime state machine

```text
CLEAN      end == stable_end
DIRTY      end > stable_end
POISONED   durability outcome uncertain after an I/O failure
```

| Event | Before | After |
| --- | --- | --- |
| append OK | CLEAN/DIRTY | DIRTY |
| logical reject | any | unchanged |
| sync OK | DIRTY | CLEAN |
| sync on CLEAN | CLEAN | CLEAN (no-op) |
| I/O failure with uncertain frontier | CLEAN/DIRTY | POISONED |
| mutation on POISONED | POISONED | `EPOISONED` |
| reopen | any | CLEAN or error |

Structural operations publish a new manifest via `CURRENT`; a crash before
publication keeps the old topology, a crash after keeps the new one. There is
no intermediate topology.

## 6. Error taxonomy

API/use: `EINVAL`, `ERANGE`, `ETOOSMALL`, `EOVERFLOW`.
State: `ESTALE`, `EGONE`, `EUNSTABLE`, `EBUSY`, `EROFS`.
Storage: `EIO`, `ECORRUPT`, `EFORMAT`, `EPOISONED`.
Lifecycle: `ENOENT`, `EEXIST`, `ENOMEM`.

## 7. Ownership and limits

One handle is caller-serialized; a writable open is exclusive and read-only
opens are shared. Record payloads are caller-owned during a call; read output
buffers are caller-owned. Maximum payload 16 MiB; maximum batch count
`UINT32_MAX`; automatic rotation target 64 MiB.
