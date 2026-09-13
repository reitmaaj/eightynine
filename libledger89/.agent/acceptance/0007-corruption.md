# Acceptance criteria: corruption classification (L7)

These criteria trace to `.agent/testing/0007-scenarios-corruption.md` and
are enforced by `test/corrupt/test_corruption.c`.

## Must exhibit (exhibit)

- Every single-byte mutation in a sealed segment header or footer MUST be
  rejected at open with `LEDGER89_ERR_CORRUPT`.
- Every single-byte mutation in the active segment's complete history MUST
  either be rejected at open with `LEDGER89_ERR_CORRUPT` (when a later
  complete batch exists) or be discarded as a torn tail (when it is the
  last batch), preserving the preceding prefix.
- Every single-byte mutation in a sealed record MUST be reported as
  `LEDGER89_ERR_CORRUPT` when that record is read or iterated; the library
  MUST NOT skip it and continue with plausible-looking data.
- A single-record read MAY succeed when only the enclosing batch framing is
  damaged and the record's own header, payload, and CRC are intact; the
  damage MUST then be reported by iteration or by any later read that
  traverses the damaged batch.
- A record with a valid checksum but a non-contiguous index MUST be
  rejected.
- Length fields MUST never cause a read beyond the segment bounds; an
  impossible length MUST yield `LEDGER89_ERR_CORRUPT`, never `IO` and never
  an out-of-bounds access.
- A valid-checksum header with nonzero flags or a valid-checksum footer with
  `last_index < first_index` MUST be rejected.
- Random bytes appended after the active tail MUST be truncated; random
  bytes inside the active prefix MUST be rejected.

## Must reject / fail safely (reject)

- Recovery MUST NOT silently skip, repair, or invent a record.
- Recovery MUST NOT continue scanning past corruption in the hope of finding
  a plausible record.
- A corrupt sealed segment MUST NOT be overwritten or truncated.
- The parser MUST NOT read outside the declared structure or segment bounds
  for any input, including wholly random files.
