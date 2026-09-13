# BDD scenarios: reference host loop

Drives `examples/host_loop.c` via `just example`.

- SCENARIO Election and leadership: GIVEN a two-node configuration WHEN the
  node times out and the peer's granted vote is delivered THEN it persists
  the vote, emits the vote requests, and becomes leader.
- SCENARIO Command lifecycle: GIVEN a leader WHEN a command is proposed and
  the peer acknowledges the AppendEntries THEN the entry is durable, applied
  once, and printed.
- SCENARIO Loopback: GIVEN the drain loop WHEN sends are emitted THEN they
  are captured by the in-process packet queue.
