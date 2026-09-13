# Acceptance criteria: golden byte fixtures (L11)

These criteria trace to `.agent/testing/0011-scenarios-golden.md` and are
enforced by `test/golden/`.

## Must exhibit (exhibit)

- Re-running the fixture sequence through the public API MUST reproduce the
  frozen segment files byte for byte; the on-disk format is frozen.
- Opening a directory seeded with the frozen fixtures MUST recover the exact
  record sequence, tags, and payload bytes without regeneration.
- Golden fixtures MUST be regenerable only by an explicit `just golden-gen`.

## Must reject / fail safely (reject)

- A single flipped byte inside a frozen fixture MUST NOT be silently ignored:
  reading the affected record MUST return `LEDGER89_ERR_CORRUPT` and MUST NOT
  return payload bytes.
- `just golden-gen` MUST fail rather than emit partial fixtures when the
  fixture sequence cannot be built or copied.
