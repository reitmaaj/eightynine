# Benchmark baseline (L12)

Recorded by `just bench` (gcc 15.3.1, `-O2`, strict C89 flags) on
2026-09-14. Host: Intel Core i7-8565U, 8 threads, btrfs for `build/`.
Workload: 50,000 records of 64-byte payloads appended in batches of 32,
`max_segment_records = 256` (about 195 rotations), one sync of the full tail,
5,000 sampled ascending reads (stride 10), then a full iteration.

| Phase | Work | Time | Rate |
| --- | --- | --- | --- |
| append | 50,000 records (1,563 batches, ~195 rotations) | 4.65 s | ~10,800 records/s |
| sync | 50,000 dirty records flushed | 0.0049 s | ~10.2M records/s |
| read | 5,000 sampled reads | 2.43 s | ~2,060 reads/s |
| iterate | 50,000 records | 0.199 s | ~251,000 records/s |

## Observations

- Appends are dominated by rotation fsyncs; with 256-record segments the
  durability cost is paid per segment, not per record.
- `sync` is cheap after appends because most bytes were already made durable
  by rotation; it only flushes the final active segment.
- Random reads are the slowest phase: v1 restarts the segment walker on every
  `ledger89_read` and issues one `pread` per record scanned, so a read costs
  O(records before the target in its segment). Bounded segments bound that
  cost; read-heavy consumers should size segments accordingly. This is a
  deliberate v1 trade-off (no in-memory index, bounded memory), not a defect.
- Iteration is streaming and linear.

## Re-running

`just bench` is reproducible: fixed record count, payload, batch size, and
segment target; no clocks or randomness in the library path. Numbers vary with
host and filesystem; update this table only with a deliberate, reviewed run.
