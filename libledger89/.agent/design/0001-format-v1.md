# libledger89 on-disk format v1

All integers are unsigned and little-endian. All checksums are CRC-32C
(Castagnoli, reflected polynomial `0x82F63B78`). No native structure is ever
written; every field is serialized explicitly. A format change requires a new
`format_version`.

## Segment header — 32 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89SEG1"` |
| 8 | 2 | format_version = 1 |
| 10 | 2 | header_size = 32 |
| 12 | 4 | flags = 0 |
| 16 | 8 | first_index |
| 24 | 4 | reserved = 0 |
| 28 | 4 | header_crc32c over bytes 0..27 |

## Record — 36 + payload_size bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 4 | magic `"R89\x01"` |
| 4 | 4 | flags = 0 |
| 8 | 8 | index |
| 16 | 8 | tag |
| 24 | 4 | payload_size (0 .. 16 MiB) |
| 28 | 4 | reserved = 0 |
| 32 | n | payload bytes |
| 32+n | 4 | record_crc32c over bytes 0..31+n |

## Batch header — 24 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 4 | magic `"B89\x01"` |
| 4 | 4 | record_count |
| 8 | 8 | first_index |
| 16 | 8 | batch_bytes (total including header and footer) |

## Batch footer — 24 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 4 | magic `"b89\x01"` |
| 4 | 4 | record_count |
| 8 | 8 | last_index |
| 16 | 4 | batch_crc32c over bytes from batch start through footer offset 19 |
| 20 | 4 | reserved = 0 |

Batch invariant: `batch_bytes = 24 + sum(36 + payload_size) + 24`;
`last_index = first_index + record_count - 1`.

## Sealed segment footer — 48 bytes

| offset | size | field |
| --- | --- | --- |
| 0 | 8 | magic `"LD89END1"` |
| 8 | 8 | last_index |
| 16 | 8 | record_count |
| 24 | 4 | segment_digest (CRC over all batch bytes) |
| 28 | 4 | reserved = 0 |
| 32 | 8 | body_size (header end .. footer start) |
| 40 | 4 | footer_crc32c over bytes 0..39 |
| 44 | 4 | reserved = 0 |

## File set

```text
lock          advisory flock file; never parsed
active.seg    mutable active segment
NNNN.seg      sealed segment; NNNN is the creation first_index (unique key)
.tmp-*        transient rewrite files; ignored by recovery
```

Segment keys are unique but are not rewritten by prefix discard; the
header's `first_index` is authoritative and recovery orders and validates
segments by header range.

Recovery ignores unknown files. A file matching the sealed-segment name
pattern with an invalid header is corruption, never silently skipped.

## Validation points

| Region | Validated at |
| --- | --- |
| segment header magic/version/size/CRC | open and read |
| sealed footer magic/size/CRC/internal consistency | open |
| header ranges: ordering, duplicates, gaps | open |
| batch header fields, record framing, batch CRC | open (active), read |
| record magic/flags/size/CRC | read and iteration |
| segment digest | written; not verified in v1 (reserved for a verify tool) |
