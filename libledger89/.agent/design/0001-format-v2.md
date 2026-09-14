# libledger89 on-disk format v2

All integers are unsigned little-endian. All checksums are CRC-32C
(reflected polynomial `0x82F63B78`, seed 0, chaining by passing the result
back). No native structure is written; every field is serialized explicitly.

## File set

```text
CURRENT                    32 bytes, atomically replaced
MANIFEST.<16hex-gen>       immutable once published
part.<16hex-file_id>       active or sealed, per manifest
CURRENT.tmp                transient publish staging
lock                       advisory flock target; never parsed
```

## CURRENT — 32 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89CUR2"` |
| 8 | 8 | manifest_generation |
| 16 | 4 | format_version = 2 |
| 20 | 4 | crc32c over bytes 0..19 |
| 24 | 8 | reserved = 0 |

## MANIFEST.<gen>

Header 64 bytes:

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89MAN2"` |
| 8 | 8 | generation |
| 16 | 16 | ledger_uuid |
| 32 | 8 | revision |
| 40 | 8 | first |
| 48 | 4 | sealed_count |
| 52 | 4 | format_version = 2 |
| 56 | 4 | header crc32c over 0..55 |
| 60 | 4 | reserved = 0 |

Then `sealed_count` descriptors of 24 bytes `{file_id u64, first u64, end u64}`,
then one active descriptor of 16 bytes `{file_id u64, first u64}`, then a
4-byte manifest crc over all preceding bytes. Sealed ranges must be
contiguous from `first`; `active.first` must equal the last sealed end (or
`first`).

## Part header — 64 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89PRT2"` |
| 8 | 16 | ledger_uuid |
| 24 | 8 | file_id |
| 32 | 8 | revision at creation |
| 40 | 8 | first |
| 48 | 4 | format_version = 2 |
| 52 | 4 | reserved = 0 |
| 56 | 4 | header crc32c over 0..55 |
| 60 | 4 | reserved = 0 |

Every part begins with a baseline `STABLE(first)` marker at offset 64.

## BATCH — one `appendv()` call

| region | size | fields |
| --- | --- | --- |
| header | 32 | magic `"B89\x02"`, count u32, first_index u64, batch_bytes u64, header crc32c, reserved |
| record × count | 8 + n | length u32, payload, payload crc32c over `length‖payload` |
| footer | 24 | magic `"b89\x02"`, count u32, last_index u64, batch crc32c over batch start..footer offset 15, reserved |

Record indices are implicit: `first_index + position`. Invariants:
`count > 0`; `last_index = first_index + count - 1`;
`batch_bytes = 32 + Σ(8 + length) + 24`.

## STABLE marker — 64 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89STB2"` |
| 8 | 16 | ledger_uuid |
| 24 | 8 | file_id |
| 32 | 8 | revision |
| 40 | 8 | end |
| 48 | 4 | format_version = 2 |
| 52 | 4 | crc32c over 0..51 |
| 56 | 8 | trailer `"89STABLE"` |

The fixed size and trailer make backward search deterministic: scan from EOF
for the trailer, then validate the 64-byte marker that starts 56 bytes
before it.

## Sealed footer — 64 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89SGF2"` |
| 8 | 16 | ledger_uuid |
| 24 | 8 | file_id |
| 32 | 8 | first |
| 40 | 8 | end |
| 48 | 8 | record_count |
| 56 | 4 | segment digest over header end..footer start |
| 60 | 4 | footer crc32c over 0..59 |

## Publication sequences

Create: `mkdir`, lock, active part (header + baseline marker) fsync,
`MANIFEST.1` fsync + dir fsync, `CURRENT.tmp` fsync, rename, dir fsync.

Structural: prepare new parts (fsync each), write `MANIFEST.<g+1>` fsync +
dir fsync, publish `CURRENT` via tmp/rename, dir fsync, then unlink obsolete
parts. Crash before publication keeps the old topology (new files are
orphans); crash after keeps the new one.

## Validation points

| Region | Validated at |
| --- | --- |
| CURRENT magic/version/crc | open |
| manifest magic/version/crc/descriptors | open |
| part headers and sealed footers | open |
| active baseline marker and forward framing to the chosen marker | open |
| record payload crc | read |
| batch crc and sealed digests | `ledger89_verify` |
