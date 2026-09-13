# Acceptance criteria: stress and throughput (L12)

These criteria trace to `.agent/testing/0012-scenarios-stress.md` and are
enforced by `test/stress/` and `just bench`.

## Must exhibit (exhibit)

- A ledger with one record per segment MUST iterate every record exactly once,
  in ascending order, with exact tags and payloads; `just long` covers
  thousands of segments.
- Recovery across many segments MUST restore the exact first/last indices and
  the exact sequence.
- A 1 MiB payload MUST round-trip byte-for-byte, and a large batch MUST remain
  atomic and ordered.
- `just bench` MUST complete without error and report append, sync, read, and
  iteration rates.

## Must reject / fail safely (reject)

- A read across a segment boundary MUST NOT return another record's bytes; a
  mismatched index or tag MUST be reported as an error, not silently accepted.
- `just bench` MUST NOT be part of `just test`; a benchmark failure MUST NOT
  mask or replace the deterministic suites.
