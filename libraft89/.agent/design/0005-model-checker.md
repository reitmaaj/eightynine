# Bounded model checker (M10)

Test: `test/exhaustive/test_model3.c`. Support: `test/support/raft_cluster.*`,
`packet_queue.*`, `invariants.*`, `raft89_inspect.*`, `trace.*`.

The checker explores a bounded state space of a three-node cluster that runs
the real library against the fake durable stores and application. It is not a
test of an expected leader or an expected commit; the assertion is that no
reachable state violates a safety invariant (I01..I20, see
`0000-design.md`).

## Search

Depth-first search with an explicit path of events and a global visited set.
The path is the replayable counterexample trace; the visited set stores a
128-bit fingerprint of the canonical state. Memory is proportional to the
path depth plus the visited set, not to the frontier. Transitions are
enumerated in protocol-first order: complete outstanding actions, recover
down nodes, deliver packets, propose, advance time, lose/duplicate/drop
packets, change links, and crash last. That ordering makes a capped run
explore protocol progress before environment noise.

Four deterministic roots are explored, most informative first: a leader with
a committed entry and a partitioned follower, a leader with a committed
entry, an elected leader, and the empty cluster. The seeds exercise
elections, replication, commit, and apply without spending the depth budget;
the DFS then explores every event sequence of the bounded length from each
root. A violation trace names the seed and the event path; replaying the seed
and the printed events reproduces the violation.

A `--strict` flag turns an inconclusive run (state cap or depth cap reached)
into exit status 2. The default run prints `INCONCLUSIVE` and exits 0.

## Canonical state

The key covers everything that can affect future behavior:

- per node: `up` flag, crash count, durable store (`H`, complete log with
  payload bytes), application state, `raft89_status`, timers, continuation
  step, candidate votes, leader peer state (`next_index`, `match_index`),
  parked AppendEntries fields, pending/out entry buffers, and the outstanding
  action with its payload;
- the packet queue in order, each packet with its complete semantic message
  and payload;
- the directed link matrix.

Action ids and the action sequence counter are excluded: they are monotonic
tokens whose absolute value cannot influence protocol behavior. `I18` is
checked transitionally instead.

## Transitions

For every reachable state the checker considers:

- `TICK(node, elapsed)` for elapsed in `{20, 10, 1}` while no action is
  outstanding;
- `DELIVER(packet)`: gated by the directed link and the destination being up,
  so packets may arrive after a heal;
- `DROP(packet)` and `DUPLICATE(packet)`;
- `CRASH(node)` before the outstanding effect (`C0`) or after it (`C1`);
- `RESTART(node)`: rebuild from the durable store;
- `PROPOSE(node, A|B)`;
- `PARTITION(a, b)` and `HEAL(a, b)` on each directed pair;
- `EFFECT(node)`: perform the durable effect of the outstanding action;
- `ACK(node, OK)` after the effect, and `ACK(node, LOST)` for a send with no
  effect.

The host oracle in `test/support/crash_oracle.*` implements the effect and
crash contract from `0001-crash-contract.md`; the checker uses it rather than
a private path.

## Bounds and pruning

Compile-time bounds (overridable with `-D`): `MODEL_MAX_TERM`,
`MODEL_MAX_INDEX`, `MODEL_MAX_PACKETS`, `MODEL_MAX_CRASHES`,
`MODEL_MAX_DEPTH`, `MODEL_STATE_CAP`, `MODEL_SEEDS`, and `MODEL_ENV`
(`0` removes environment transitions for a pure protocol run). A transition
whose result would exceed a term or index bound is not taken. Reaching the
state or depth cap marks the run inconclusive; it is never reported as a
pass.

## Invariant mapping

| Invariant | Checked |
|---|---|
| I01 one leader per term | per state |
| I02 monotonic terms | parent/child |
| I03 one vote per term | parent/child (a durable vote never changes within a term) |
| I04 valid voted_for | per state |
| I05 log matching | per state |
| I06 leader append-only | parent/child |
| I07 leader completeness | per state |
| I08 no divergent applies | per state |
| I09 ordered apply | per state |
| I10 applied <= commit <= last | per state |
| I11 monotonic commit | parent/child |
| I12 monotonic applied | parent/child |
| I13 current-term quorum commit | parent/child plus peer state |
| I14 barrier (no acknowledged lead) | per state |
| I15 no success before durability | per state |
| I16 no apply before commit | per state |
| I17 one outstanding action | per state |
| I18 stable action id until ack | transition (ack path) |
| I19 monotonic match_index | parent/child |
| I20 stale responses cannot regress | parent/child plus stale delivery |

## Results

Recorded run (default bounds: term 3, index 3, 6 packets, 2 crashes,
depth 6, cap 500000):

```text
states=205237 edges=648827 depth=6 leader=480441 logs=988083 commits=958312
applies=971745 down=220724 packets=5 result=pass
```

`just exhaustive-sanitize` runs the same exploration under ASan and UBSan.
Raising `MODEL_MAX_DEPTH` widens the exploration and usually ends
`INCONCLUSIVE` at the state cap, which is reported honestly and exits 0
(exit 2 under `--strict`).
