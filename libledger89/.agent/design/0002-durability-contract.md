# libledger89 durability contract (v2)

This document is normative for the durability guarantees and the test oracle.

## 1. Durability states

```text
logical   records visible through the public API in this process
durable   records recoverable after process crash, OS crash, or power loss
```

`appendv` makes a batch logically visible on success. `sync` writes a stable
marker and flushes it; after success `stable_end == end` and the prefix
`[first, stable_end)` is recoverable, subject to the filesystem honoring the
required primitives.

## 2. Primitives

| Operation | File sync | Directory sync |
| --- | --- | --- |
| appendv | none (deferred) | no |
| sync | write marker, then fdatasync(active) | no |
| rotate | footer fsync, new part fsync, manifest fsync | before CURRENT rename and after |
| truncate/prune | new part fsyncs, manifest fsync | before CURRENT rename, after GC unlink |

Structural operations are self-durable: each step is crash-safe and returns
only when its result is durable.

## 3. Crash-state rule

For any operation and crash point the recovered ledger is one of the states
explicitly permitted for that operation. No crash exposes a partial batch, a
gap or duplicate, a mutated sealed part, a record absent from both pre- and
post-states, or silently skipped stable history.

## 4. Sync crash outcomes

| Crash point | Durable state | Recovered `stable_end` |
| --- | --- | --- |
| before marker write | old marker | old end |
| mid-marker write (torn) | partial marker | old end |
| marker written, flush incomplete | marker maybe present | old or new, by marker validity |
| flush returned, sync not returned | marker durable | new end |
| after sync returns | marker durable | new end guaranteed |
| I/O error | unknown | reopen decides; handle poisoned meanwhile |

A failed `sync()` may still have reached storage completely; recovery accepts
`STABLE(end)` only if its prefix validates.

## 5. Torn tail versus corruption

Recovery finds the latest syntactically valid stable marker and validates the
active prefix forward from the part header to that marker. Bytes after it are
disposable. Any framing corruption before or at the chosen marker is
`ECORRUPT` and is never repaired. Payload corruption in stable history is
detected at `read` or `verify` and is also `ECORRUPT`.

## 6. Faulted handles

Any I/O failure whose durable outcome is uncertain poisons a writable handle.
Subsequent mutations return `EPOISONED`; reads may still be attempted. Close
and reopen recovers. `EINTR` is retried transparently.

## 7. Named crash properties

```text
CR01 no partial batch is ever visible after recovery
CR02 a successful sync makes all preceding appends durable
CR03 recovery never skips corruption before the stable frontier
CR04 sealed parts and published manifests never change
CR05 rotation preserves the exact logical sequence
CR06 truncation never creates a gap and never resurrects a discarded index
CR07 pruning never removes a retained record and never changes revision
CR08 repeated recovery is idempotent
CR09 a poisoned handle never reports success for a mutation
CR10 parsing never trusts unchecked lengths, offsets, or counts
CR11 every crash resolves to one published topology plus one stable prefix
```
