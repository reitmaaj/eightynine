# Acceptance criteria: format and checksums (L0)

These criteria trace to `.agent/testing/0000-scenarios-format.md` and are
enforced by `test/unit/test_crc.c` and `test/unit/test_format.c`.

## Must exhibit (exhibit)

- CRC-32C MUST return `0x00000000` for empty input, `0xE3069283` for
  `123456789`, and `0x8A9136AA` for 32 zero bytes.
- CRC-32C MUST be chunk-independent: incremental updates MUST equal the
  one-shot result for any split.
- Scalar codecs MUST serialize little-endian and round-trip exactly.
- Every structure encoder MUST produce the canonical bytes of
  `.agent/design/0001-format-v1.md`, and re-encoding a decoded structure MUST
  reproduce the input bytes.
- Segment header, segment footer, record header, batch header, and batch
  footer decoders MUST reject bad magic, bad version, reserved bits, and
  out-of-range lengths with `LEDGER89_ERR_CORRUPT`.
- Segment header and segment footer decoders MUST validate their own CRC
  fields.
- Width conversion MUST reject values above `ULONG_MAX` with
  `LEDGER89_ERR_RANGE` and MUST NOT modify the output.

## Must reject / fail safely (reject)

- A decoder MUST NOT read beyond the fixed structure size it is given.
- A decoder MUST NOT accept an unchecked `payload_size` or `batch_bytes`;
  impossible values MUST be rejected before any dependent arithmetic.
- A corrupted CRC field MUST NOT be accepted, and no decoder may "repair" a
  structure silently.
- No format function may allocate memory or perform I/O.
