# BDD scenarios: stress and throughput (L12)

These scenarios drive `test/stress/` and the `just bench` harness. IDs
cross-reference `.agent/acceptance/0012-stress.md`.

- SCENARIO Many tiny segments: GIVEN `max_segment_records == 1` WHEN hundreds
  of records are appended THEN each record occupies its own sealed segment,
  iteration yields every record exactly once in order, and reads across
  segment boundaries return exact payloads (ST01). `just long` repeats the
  sweep with thousands of segments.
- SCENARIO Huge payload and large batch: GIVEN a 1 MiB binary payload and a
  multi-thousand-record batch in one segment WHEN read and iterated THEN
  boundary bytes and tags are exact (ST02).
- SCENARIO Reopen across many segments: GIVEN a ledger with hundreds of
  segments WHEN closed and reopened THEN recovery restores the exact sequence
  (ST03).
- SCENARIO Bench harness: GIVEN a built library WHEN `just bench` runs THEN it
  reports append, sync, read, and iteration rates and exits zero (ST04).
