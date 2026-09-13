# BDD scenarios: follower AppendEntries, commit, and apply

Drives `test/unit/test_follower.c` and `test/unit/test_apply.c`.

## Prefix validation

- SCENARIO Empty prefix: GIVEN `prev_log_index = 0` WHEN an AppendEntries
  arrives THEN the prefix matches (F01).
- SCENARIO Existing prefix: GIVEN a local entry matching
  `(prev_log_index, prev_log_term)` WHEN checked THEN it matches (F02).
- SCENARIO Missing prefix: GIVEN `prev_log_index` beyond the local log WHEN
  checked THEN the RPC fails (F03).
- SCENARIO Wrong term: GIVEN a local entry at `prev_log_index` with a
  different term WHEN checked THEN the RPC fails (F04).
- SCENARIO Failure mutates nothing: GIVEN a failed consistency check THEN
  the durable log is unchanged and no log action is emitted (F05, F06).

## Appending

- SCENARIO Append to an empty log: GIVEN a matching empty prefix and one
  entry WHEN processed THEN a `LOG_APPEND` action appears and the durable
  log gains the entry after acknowledgement (F07, F08).
- SCENARIO Append several: GIVEN several consecutive entries THEN one
  `LOG_APPEND` carries all of them (F09).
- SCENARIO Heartbeat: GIVEN zero entries WHEN processed THEN no log action
  is emitted and the reply succeeds (F10).
- SCENARIO Already present: GIVEN all entries already stored identically
  THEN no rewrite is requested (F11).
- SCENARIO Suffix only: GIVEN a matching prefix and a new suffix THEN the
  `LOG_APPEND` starts at the first missing index (F12).

## Conflicts

Given `follower: 1:a 2:a 3:b 4:b 5:c` and
`leader: 1:a 2:a 3:b 4:d 5:d`:

- SCENARIO Conflict at first new entry: THEN `LOG_TRUNCATE(4)` is
  acknowledged before `LOG_APPEND(4:d,5:d)` (F13, F19).
- SCENARIO Conflict in the middle: GIVEN the conflict starts at entry four
  of a longer batch THEN the truncate boundary is the first divergent index
  (F14).
- SCENARIO Conflict at final entry: GIVEN the conflict is the last entry
  THEN the truncate boundary is that entry (F15).
- SCENARIO Full suffix removal: GIVEN a conflicting suffix THEN every
  divergent entry is removed before the replacement append (F16).
- SCENARIO Prefix retained: GIVEN matching entries before the conflict THEN
  they are retained byte-for-byte (F17).
- SCENARIO Durability before success: GIVEN truncate and append actions
  THEN a successful response is emitted only after both are acknowledged
  (F19).

## Commit and apply

- SCENARIO No regression: GIVEN a `leader_commit` below the local
  `commit_index` THEN no change (F20).
- SCENARIO Equal commit: GIVEN an equal `leader_commit` THEN no change
  (F21).
- SCENARIO Clamp: GIVEN `leader_commit` beyond the matched log THEN commit
  is clamped to the matched index (F22).
- SCENARIO Sequential apply: GIVEN newly committed entries THEN `APPLY`
  actions appear in index order without gaps (F23, P01, P02).
- SCENARIO One apply at a time: GIVEN several committed entries THEN
  exactly one `APPLY` is outstanding at any time (F24, P03).
- SCENARIO Persistence before application: GIVEN appended entries that
  become committed THEN application starts only after the append
  acknowledgement (F25).
- SCENARIO Heartbeat commit: GIVEN a heartbeat with a higher
  `leader_commit` THEN commit advances and applies (F26).
- SCENARIO Payload fidelity: GIVEN an applied entry THEN its term, index,
  and payload equal the durable log entry (P04, P05).
- SCENARIO Fatal apply: GIVEN an `APPLY` acknowledged `FATAL` THEN the node
  faults (P06).
