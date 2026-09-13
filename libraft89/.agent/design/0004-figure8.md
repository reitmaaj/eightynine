# libraft89: M9 figure-8 simulation

Test: `test/sim/test_figure8_sim.c`.

Five nodes share the real library and fake durable stores. The harness
delivers semantic messages through a queue gated by a directed partition
map, drains actions in order, and records every applied `(index, payload)`.

Sequence: S1 leads term 1 and commits `A` at index 1; the cluster partitions
`{1,2}` from `{3,4,5}`; S1 appends `X` at index 2 and replicates it to S2
only. S1 crashes; S5 leads with `Y` at index 2 but crashes before
replicating. S1 restarts and wins an election, replicating `X` to S3: `X`
is now on a majority, but it belongs to an old term, so commit must not
advance. S1 crashes; S5 returns, wins a later election, overwrites index 2
with `Y`, commits a current-term entry, and applies `Y`.

Assertions: no node ever applied `X` at index 2; index 2 is applied as `Y`;
index 1 is applied as `A` everywhere.
