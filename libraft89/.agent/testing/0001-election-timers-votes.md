# BDD scenarios: action protocol, timers, elections, and votes

Drives `test/unit/test_actions.c`, `test/unit/test_timers.c`,
`test/unit/test_election.c`, and `test/unit/test_vote.c`.

## Action protocol

- SCENARIO Action ids: GIVEN a node that emits an action WHEN the action is
  observed THEN its id is non-zero and increases by one per action (A02).
- SCENARIO Repeated peek: GIVEN an outstanding action WHEN
  `raft89_next_action` is called repeatedly THEN the same id, type, and
  content are returned each time (A03).
- SCENARIO Tick while pending: GIVEN an outstanding action WHEN
  `raft89_tick` runs THEN `RAFT89_BUSY` (A04).
- SCENARIO Receive while pending: GIVEN an outstanding action WHEN
  `raft89_recv` runs THEN `RAFT89_BUSY` (A05).
- SCENARIO Lost send: GIVEN an outstanding `SEND` WHEN acknowledged with
  `RAFT89_ACTION_LOST` THEN the ack succeeds and the sequence continues
  (A09).
- SCENARIO Lost storage action: GIVEN an outstanding `HARD_STATE` WHEN
  acknowledged with `RAFT89_ACTION_LOST` THEN `RAFT89_ERR_STATE` and the
  action remains outstanding (A10).
- SCENARIO Fatal action: GIVEN an outstanding action WHEN acknowledged with
  `RAFT89_ACTION_FATAL` THEN the node faults (A12).
- SCENARIO Tick after fault: GIVEN a faulted node WHEN `raft89_tick` runs
  THEN `RAFT89_ERR_FAULTED` (A13).
- SCENARIO Receive after fault: GIVEN a faulted node WHEN `raft89_recv`
  runs THEN `RAFT89_ERR_FAULTED` (A14).
- SCENARIO Status after fault: GIVEN a faulted node WHEN status is queried
  THEN it remains readable and reports `faulted != 0` (A16).
- SCENARIO Destroy after fault: GIVEN a faulted node WHEN destroyed THEN it
  succeeds safely (A17).
- SCENARIO Acknowledgement creates the next action: GIVEN an acknowledged
  `HARD_STATE` for an election WHEN the ack returns THEN the first
  `RequestVote` action is immediately observable (A18).

## Timers

- SCENARIO Below timeout: GIVEN a follower and a tick below the election
  timeout WHEN ticked THEN no action (T01).
- SCENARIO Reaching timeout: GIVEN a follower and a tick reaching the
  election timeout WHEN ticked THEN a `HARD_STATE` election action (T02).
- SCENARIO One transition per tick: GIVEN a tick far beyond the timeout
  WHEN processed THEN exactly one election transition (T03).
- SCENARIO Minimum timeout: GIVEN the random source yields 0 WHEN the
  election timeout is chosen THEN it equals `election_timeout_min` (T04).
- SCENARIO Maximum timeout: GIVEN the random source yields the range
  maximum WHEN chosen THEN it equals `election_timeout_max` (T05).
- SCENARIO Accumulated ticks: GIVEN repeated small ticks WHEN their sum
  reaches the timeout THEN one election transition (T10).
- SCENARIO Time saturation: GIVEN a tick near the maximum time value WHEN
  processed THEN time saturates, the node does not fault, and no premature
  wrap occurs (T11).

## Elections

- SCENARIO Follower times out: GIVEN a follower WHEN its election timer
  expires THEN it becomes a candidate (E01).
- SCENARIO Term increment: GIVEN an election WHEN the hard state is
  acknowledged THEN the term increased exactly once (E02).
- SCENARIO Self vote: GIVEN a new election THEN the persisted vote is the
  node itself (E03).
- SCENARIO Hard state first: GIVEN an election THEN the first action is
  `HARD_STATE` and only after its acknowledgement are `RequestVote`
  messages emitted (E04).
- SCENARIO One request per peer: GIVEN `N` members WHEN the election
  broadcasts THEN exactly `N-1` `RequestVote` messages are sent, one per
  other member (E05).
- SCENARIO No self request: GIVEN an election THEN no `RequestVote` is sent
  to the candidate itself (E06).
- SCENARIO Majority wins: GIVEN a quorum of granted votes WHEN counted
  THEN the candidate becomes leader (E07).
- SCENARIO Minority stays candidate: GIVEN fewer than a quorum of granted
  votes THEN the candidate remains a candidate (E08).
- SCENARIO Duplicate votes: GIVEN the same peer's positive vote twice THEN
  it is counted once and cannot form a false majority (E09).
- SCENARIO Negative vote: GIVEN a refusal from a peer THEN it is not
  counted (E10).
- SCENARIO Higher-term response: GIVEN a candidate WHEN a response carries
  a higher term THEN it becomes a follower (E11).
- SCENARIO Higher-term durability: GIVEN a higher term is observed THEN the
  new hard state is persisted before any further message is emitted (E12).
- SCENARIO Repeated election: GIVEN a candidate whose election timer
  expires again THEN a new election starts in a higher term (E15).
- SCENARIO Single node: GIVEN a one-node cluster WHEN the election hard
  state is acknowledged THEN the node becomes leader immediately (E16).
- SCENARIO Quorum sizes: GIVEN two, three, four, and five member clusters
  WHEN votes arrive THEN a quorum is one, two, three, and three grants
  including the self vote (E17-E20).

## RequestVote

- SCENARIO Stale request: GIVEN a request with a lower term THEN the reply
  refuses and carries the current term, with no persistent change (V01).
- SCENARIO Fresh grant: GIVEN the current term, no prior vote, and a fresh
  candidate log THEN the reply grants the vote (V02).
- SCENARIO Stale candidate log: GIVEN the current term and a candidate log
  older than the local log THEN the reply refuses (V03).
- SCENARIO Repeated request: GIVEN the same candidate already granted in
  this term THEN the reply grants again (V04).
- SCENARIO Other candidate: GIVEN a different candidate already granted in
  this term THEN the reply refuses (V05).
- SCENARIO Higher term: GIVEN a higher-term request THEN hard state is
  persisted before the reply is emitted (V06).
- SCENARIO Higher term clears vote: GIVEN an old vote WHEN a higher-term
  request is refused for log freshness THEN the persisted vote becomes
  none before the reply (V07).
- SCENARIO Atomic vote persistence: GIVEN a granted vote THEN the
  `HARD_STATE` action carries both the new term and the candidate (V08).
- SCENARIO Duplicate request stability: GIVEN repeated identical requests
  THEN no contradictory hard state is ever emitted (V09).
- SCENARIO Unknown sender: GIVEN a sender that is not a configured member
  THEN `RAFT89_ERR_PROTOCOL` and no state change (V10).
- SCENARIO Invalid boolean: GIVEN a response whose grant value is not 0 or
  1 THEN `RAFT89_ERR_PROTOCOL` (V11).
- SCENARIO Response from unknown node: GIVEN a vote response whose sender
  is not a member THEN `RAFT89_ERR_PROTOCOL` (V12).
- SCENARIO Own vote counted once: GIVEN a three-node election THEN the self
  vote plus one grant forms the quorum (V14).
