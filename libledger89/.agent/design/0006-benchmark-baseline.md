# Benchmark baseline (v2)

Recorded from `just bench` on this host (GCC 15.3.1, Linux, x86_64).
`just bench` reports three baselines: throughput, cold open/recovery, and
iteration scaling.

## Throughput

1562 batches of 32 records with 64-byte payloads, one sync, then reads and
iteration over all records:

```text
append:  49984 records in 0.365s (~136989 rec/s)
sync:    0.033s
read:    49984 records in 0.555s (~90002 rec/s)
iterate: 49984 records in 0.030s (~1650337 rec/s)
```

Append writes one batch per call with no per-batch fsync; the single sync
dominates durability cost. Read performs a binary search over the in-memory
batch directory plus one part read per record. Iteration retains a physical
cursor, so it advances linearly per record instead of rescanning each batch
from its start.

## Iteration scaling

One batch per size, 64-byte records, size-only and payload-copy iteration:

```text
 records    null_ms    data_ms  null_ns/rec  data_ns/rec
    1000      0.892      3.475        892.4       3475.0
    4000      2.304      9.678        575.9       2419.5
   16000      9.856     42.137        616.0       2633.5
```

Per-record cost is flat from 1K to 16K records per batch: no quadratic
record-offset rediscovery.

## Cold open and recovery

One 64-byte record per batch; open time after close:

```text
  batches  parts   build_s   open-ro_s   open-rw_s
     1000      1     0.041       0.002        0.003
    10000      1     0.152       0.020        0.021
   100000      1     1.226       0.217        0.201
    10000     10     0.591       0.025        0.024
    10000    100     3.748       0.027        0.027
    10000   1000    10.446       0.031        0.084
```

Open and recovery cost is proportional to retained physical structure
(batches and parts), not just manifest size. Within this envelope the cost
is approximately linear and acceptable; the measured envelope is documented
rather than guaranteed.

## Re-running

```text
just bench
```
