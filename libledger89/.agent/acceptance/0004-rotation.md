# Acceptance criteria: rotation and sealed immutability (L4)

These criteria trace to `.agent/testing/0004-scenarios-rotation.md` and are
enforced by `test/api/test_rotate.c`.

## Must exhibit (exhibit)

- `ledger89_rotate` on an empty active segment MUST be a no-op.
- Rotation MUST preserve the exact logical sequence, and a reopen MUST
  restore it from the sealed segments plus the new active segment.
- Automatic rotation MUST happen before a batch that would exceed
  `max_segment_records` or `max_segment_bytes`; a batch MUST NOT split across
  segments.
- A single batch larger than the byte target MUST occupy a fresh segment by
  itself.
- Sealed segment files MUST be byte-identical before and after subsequent
  appends and rotations.
- Reads and iteration MUST cross segment boundaries without gaps or
  duplicates.

## Must reject / fail safely (reject)

- Rotation MUST NOT lose, duplicate, or reorder records.
- A rotation failure MUST fault the handle; a reopen MUST recover either the
  pre-rotation or post-rotation state, never a gap.
- A sealed segment MUST never be written after sealing.
