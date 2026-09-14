# Benchmark baseline (v2)

Recorded from `just bench` on this host (GCC 15.3.1, Linux). The bench writes
1562 batches of 32 records with 64-byte payloads, syncs once, then reads and
iterates all records.

```text
append:  49984 records in 0.619s (~80749 rec/s)
sync:    0.034s
read:    49984 records in 0.712s (~70251 rec/s)
iterate: 49984 records in 0.578s (~86454 rec/s)
```

## Observations

- Append writes one batch per call with no per-batch fsync; the single sync
  dominates durability cost.
- Read and iteration perform a binary search over the in-memory batch
  directory plus one part read per record.

## Re-running

```text
just bench
```
