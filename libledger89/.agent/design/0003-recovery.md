# libledger89 recovery contract (v2)

## 1. Open-time algorithm

```text
recover(path, writable):
    if CURRENT missing:
        if CREATE and writable and only known files exist:
            remove orphans; create ledger
        else: ENOENT or ECORRUPT
    read and validate CURRENT; select MANIFEST(CURRENT.generation)
    if the manifest is missing or invalid: ECORRUPT   # never fall back
    if version != 2: EFORMAT
    validate descriptors: ranges contiguous from manifest.first,
        referenced parts exist, headers match, sealed footers match
    build the batch directory for every sealed part (framing only)
    marker = latest valid STABLE marker in the active part
    if none: ECORRUPT
    validate the active prefix forward from offset 64 to the marker:
        expected = active.first
        each batch must start at expected and advance it by count
        each earlier marker must end at expected
        at the chosen marker: marker.end == expected and revision matches
    state = {uuid, revision, first, stable_end = marker.end, end = marker.end}
    if writable:
        physically truncate the active part after the marker; fsync
        garbage-collect unreferenced parts, obsolete manifests, and
        CURRENT.tmp; only the generation CURRENT names is retained
    return CLEAN
```

## 2. Marker search

Scan chunks backward from EOF for the 8-byte trailer `"89STABLE"`. For each
occurrence, the marker starts 56 bytes before the trailer; validate magic,
version, crc, uuid, file_id, and revision. The first valid marker is the
frontier. A marker that fails any check is treated as torn and skipped.

## 3. Recovery outcomes

| On-disk condition | Result |
| --- | --- |
| torn batch after the last marker | discard |
| complete unsynced batch after the last marker | discard |
| torn or invalid marker at the tail | ignore; use the previous marker |
| valid latest marker | recover through its end |
| framing corruption before the chosen marker | ECORRUPT |
| corrupt or missing manifest referenced by CURRENT | ECORRUPT (no fallback) |
| orphan newer manifest or orphan part | ignore; GC when writable |
| obsolete manifest generations | unlinked best-effort by writable recovery; the CURRENT generation always remains |
| interrupted truncate/prune/rotate before publication | old topology |
| after publication | new topology (truncate also bumps revision) |
| payload corruption in stable history | ECORRUPT on read/verify |

## 4. Structural transitions

Every structural operation is copy-on-write: new part files are written and
fsynced, a new manifest is written and fsynced, `CURRENT` is atomically
replaced, and only then are obsolete parts unlinked. A crash at any point
resolves to exactly one published topology. `CURRENT` alone chooses the
manifest; greater-generation manifests are orphans.

## 5. Idempotence

Recovery is idempotent: reopening a recovered ledger performs no additional
logical change, and the writable tail cleanup is safe to repeat.

Writable recovery retains exactly one manifest generation, the one CURRENT
names. Obsolete generations are removed best-effort during recovery; a failed
unlink leaves an unreferenced file behind but never affects the manifest
CURRENT names, so recoverability is independent of cleanup success.
