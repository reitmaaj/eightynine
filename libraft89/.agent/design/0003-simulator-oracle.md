# Deterministic simulator and crash oracle

The test architecture treats libraft89 as a deterministic safety-critical
state machine. Every transition runs the global Raft invariants, so a
scenario that nominally tests elections can still fail on log safety, action
ordering, or durability.

## Layers

```text
test/
    smoke.c
    unit/        one program per concern
    crash/       durable-effect semantics at C0/CX/C1/C2
    sim/         multi-node scenarios, Figure-8, and seeded random traces
    exhaustive/  bounded model exploration
    failalloc/   allocation-failure injection (GNU ld --wrap)
    support/
        fake_store.[ch]      durable H and L, fault injection
        fake_app.[ch]        durable X plus dedup table and apply trace
        fake_random.[ch]     scripted and seeded sources
        fixture.[ch]         config, store, and random bundle
        packet_queue.[ch]    deep-copied messages
        raft_cluster.[ch]    nodes, links, logical time, event primitives
        sim_event.[ch]       event collection, application, print, parse
        crash_oracle.[ch]    phases, crash points, recovery checks
        invariants.[ch]      I01..I20 over stores, status, and traces
        raft89_inspect.[ch]  test-only access, canonical snapshots, clone
        prng.[ch]            deterministic random source
        driver.[ch]          message builders and action helpers
```

No alternate transport or storage path exists for simulated nodes: the
simulator links the real library.

The model checker (`test/exhaustive/test_model3.c`) and the random
simulator (`test/sim/test_random_sim.c`) share `raft_cluster`, `sim_event`,
`invariants`, and `raft89_inspect`. The Figure-8 test uses the same cluster
and packet queue.

## Event driver

```text
TICK(node, elapsed)
DELIVER(packet)
DROP(packet)
DUPLICATE(packet)
CRASH(node, point, partial_token)
RESTART(node)
PROPOSE(node, command)
PARTITION(a, b) / HEAL(a, b)
EFFECT_BEGIN / EFFECT_FINISH / ACK
```

`DELIVER` is gated by the current directed adjacency at delivery time, so a
packet may sit in flight across a partition and arrive after a heal. This is
required for the stale-message scenarios.

Actions drain automatically through the oracle, except when a scenario wants
manual control to stop at C0, CX, or C1.

## Packet ownership

`RAFT89_ACT_SEND` payloads remain valid only until acknowledgement, so the
oracle deep-copies the semantic message into the packet queue at effect
time. Queued packets are test-owned; `raft89_recv` copies what it needs
before returning.

## Invariant engine

After every event, for every node: snapshot status and durable store, then
check I01..I20. `raft89_inspect` exposes leader peer state (`next_index`,
`match_index`) and timer values; it includes `src/raft89_internal.h` and is
never part of the release archive or the public header.

## Exhaustive exploration

`test/exhaustive/test_model3.c` canonicalizes cluster states (durable
stores, volatile status, packet multiset, partitions, oracle phase) and runs
breadth-first over the transition set above. Bounds: 3 nodes, commands
`{A,B}`, max term 3, max index 3, at most 6 packets, at most 2 crashes per
node. The important assertion is never "expected leader is 2" but "no
reachable state violates a safety invariant".

## Random traces

`test/sim/test_random_sim.c` draws events from a recorded seed, weighting
reordering, duplication, stale responses, crashes, and one-way partitions.
On failure it prints a replayable trace; a later milestone adds
delta-debugging shrinking. Same seed plus same event list always reproduces
the same actions.
