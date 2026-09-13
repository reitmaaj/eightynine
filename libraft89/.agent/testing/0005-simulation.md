# BDD scenarios: deterministic simulation

Drives `test/sim/test_*.c`. The simulator links the real library, owns
durable stores, application state, and a packet queue, and gates delivery
by a directed partition map.

- SCENARIO Clean election: GIVEN five reachable nodes with empty logs WHEN
  one times out THEN it wins a quorum and becomes leader.
- SCENARIO Replicated proposal: GIVEN a leader WHEN a command is proposed
  and delivered to a majority THEN every reachable node applies the same
  command at the same index.
- SCENARIO Isolated leader: GIVEN a leader partitioned from the majority
  WHEN it proposes THEN the entry is not committed and not applied.
- SCENARIO Old leader returns: GIVEN a stale leader with an uncommitted
  suffix WHEN it rejoins THEN it steps down and its suffix is repaired.
- SCENARIO Figure-8 old-term majority: GIVEN an old-term entry replicated
  to a majority after a new leader takes power THEN commit must not advance
  and no node may apply that entry; only a current-term entry may commit it
  indirectly.
