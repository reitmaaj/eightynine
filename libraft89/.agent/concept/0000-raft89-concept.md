# libraft89 concept

`libraft89` is a small, standalone ISO C89 library for the *Raft consensus
protocol* as a deterministic state machine. It decides who leads, what
belongs in the replicated log, what is committed, and in which order
committed entries reach the application. It performs no I/O and owns no
host resources.

## Position

```text
            application
        +-----------------+
        | storage/network |
        | timers/FSM      |
        +--------+--------+
                 | events / actions
        +--------v--------+
        |    libraft89    |
        | deterministic   |
        | Raft protocol   |
        +-----------------+
```

`libraft89` owns only the protocol:

- follower, candidate, and leader roles;
- elections and votes, including the log-freshness restriction;
- `AppendEntries` replication and conflict repair;
- commit calculation with the current-term restriction;
- ordered application of committed entries.

`libraft89` deliberately knows nothing about sockets, wire formats, files,
databases, threads, clocks, cluster discovery, or application semantics.
Consumers (for example a ledger daemon) embed it and interpret the one
ordered committed command stream it produces.

## Four boundaries

1. **Messages in** — `raft89_recv` delivers one semantic Raft message.
2. **Commands in** — `raft89_propose` offers one command to the leader.
3. **Storage reads** — `raft89_store` callbacks answer questions about
   durable hard state and the log. The library never writes through them.
4. **Actions out** — sends, hard-state persistence, log append/truncate, and
   application leave as `raft89_action` values, each acknowledged by the
   host before the next event.

## Invariants

The library is built around the Raft safety invariants: one leader per term;
monotonic terms; one vote per term; log matching; leader append-only; leader
completeness; state-machine safety; ordered application; and quorum-only
commitment through current-term entries. `spec/raft89-spec.md` numbers them
and `.agent/design/0001-crash-contract.md` fixes their crash semantics.

## Determinism

Given the same configuration, the same durable store contents, the same
random stream, and the same event sequence, libraft89 emits exactly the same
actions. Wall-clock time and entropy are injected, never sampled. This is
what makes the simulator, crash oracle, and bounded model checker possible.

## v1 scope

Included: static membership, opaque log entries, one outstanding action,
atomic hard-state persistence, prefix-safe log effects, and at-least-once
application keyed by log index.

Excluded: dynamic membership (joint consensus), snapshots and log
compaction, pre-vote, leadership transfer, ReadIndex/leases, transport,
serialization, threads, clocks, and storage integration.
