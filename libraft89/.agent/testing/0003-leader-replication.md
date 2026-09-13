# BDD scenarios: leader replication and commit rule

Drives `test/unit/test_leader.c`.

## Proposal and local append

- SCENARIO Leader proposal: GIVEN a leader WHEN a command is proposed THEN
  it receives the next sequential index and the current term (R02, R03).
- SCENARIO Append before replicate: GIVEN a proposal THEN `LOG_APPEND` is
  emitted before any replication `SEND` (R04).
- SCENARIO No replication before durability: GIVEN a local append that is
  not acknowledged THEN no `AppendEntries` carrying the entry is emitted
  (R05).
- SCENARIO Replicate after durability: GIVEN an acknowledged local append
  THEN `AppendEntries` for every peer follows (R06).
- SCENARIO Non-leader proposal: GIVEN a follower or candidate WHEN a
  command is proposed THEN `RAFT89_NOT_LEADER` and no state change.

## Leader state and responses

- SCENARIO Initial heartbeat: GIVEN a new leader THEN an `AppendEntries`
  is emitted to every peer (R01).
- SCENARIO Match advance: GIVEN a successful response with a higher
  `match_index` THEN replication state advances for that peer (R07, R08).
- SCENARIO Duplicate success: GIVEN the same success twice THEN it does not
  advance twice and cannot double count toward a quorum (R09).
- SCENARIO Stale success: GIVEN a success with a lower `match_index` than
  already recorded THEN replication state does not regress (R10).
- SCENARIO Failure backoff: GIVEN a failed response THEN `next_index` backs
  toward the matching prefix and another `AppendEntries` is sent (R11).
- SCENARIO Repeated failures: GIVEN repeated failures THEN the leader
  eventually discovers the common prefix and catches the peer up (R12).
- SCENARIO Batch bounds: GIVEN a long log WHEN replicating THEN the number
  of entries and the aggregate bytes respect the configured bounds (R15,
  R16).
- SCENARIO Caught-up heartbeat: GIVEN a peer at the leader's log end THEN
  the heartbeat carries zero entries (R17).
- SCENARIO Leader append-only: GIVEN a leader during its tenure THEN it
  never emits `LOG_TRUNCATE` for its own log (R18).
- SCENARIO Higher-term response: GIVEN a response with a higher term THEN
  the leader steps down and persists the new term (R19, R20).

## Commit rule

- SCENARIO Current-term quorum: GIVEN an entry from the current term on a
  majority THEN commit advances through it (K01, K08).
- SCENARIO Old-term quorum alone: GIVEN only old-term entries on a
  majority THEN commit does not advance (K02).
- SCENARIO Indirect commit: GIVEN a later current-term entry commits THEN
  all preceding entries commit with it (K03).
- SCENARIO Minority: GIVEN replication to fewer than a quorum THEN commit
  does not advance (K04).
- SCENARIO Self counts: GIVEN the leader's own log THEN it counts once
  toward the quorum (K05).
- SCENARIO Duplicate acknowledgement: GIVEN the same peer acknowledged
  twice THEN it counts once (K06).
- SCENARIO Non-member: GIVEN a sender outside the membership THEN the
  message is rejected and cannot contribute (K07).
- SCENARIO Monotonic commit: GIVEN any sequence of responses THEN
  `commit_index` never decreases (K09).
