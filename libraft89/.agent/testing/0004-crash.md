# BDD scenarios: crash points

Drives `test/crash/test_*.c` using `test/support/crash_oracle.{c,h}` and
`test/support/fake_app.{c,h}`.

## Hard state

- SCENARIO C0: GIVEN an issued `HARD_STATE` WHEN the process crashes before
  the effect THEN the old hard state survives (CR01).
- SCENARIO CX old: GIVEN a crash during persistence WHEN the old value
  survived THEN the term and vote are the old pair (CR01).
- SCENARIO CX new: GIVEN a crash during persistence WHEN the complete new
  value survived THEN the term and vote are the new pair (CR01).
- SCENARIO C1/C2: GIVEN the effect completed, with or without
  acknowledgement THEN restart sees the new hard state (CR12).
- SCENARIO No vote escape: GIVEN a granted vote THEN no response can be
  emitted before the hard-state acknowledgement (CR02).

## Log append

- SCENARIO C0: GIVEN an issued `LOG_APPEND` WHEN crashed before the effect
  THEN the log is unchanged (CR03).
- SCENARIO Partial prefix: GIVEN a crash during the append THEN recovery
  exposes any complete prefix of the batch (CR03).
- SCENARIO C1/C2: GIVEN the append completed THEN restart exposes the full
  batch, and no success response escaped before acknowledgement (CR04).

## Log truncate

- SCENARIO C0: GIVEN an issued `LOG_TRUNCATE` WHEN crashed before the
  effect THEN the suffix is intact (CR05).
- SCENARIO Partial removal: GIVEN a crash during truncation THEN recovery
  ends at a boundary between the requested first index and the old end
  (CR05).
- SCENARIO C1: GIVEN the truncate completed THEN restart ends exactly at
  `first_index - 1`.

## Application

- SCENARIO C0: GIVEN an issued `APPLY` WHEN crashed before the effect THEN
  the application state is unchanged (CR09).
- SCENARIO C1: GIVEN the application completed without acknowledgement
  THEN restart replays the entry (CR10).
- SCENARIO Replay identity: GIVEN a replayed index THEN the term and payload
  are identical and the replay is a no-op (CR11).
- SCENARIO Conflicting replay: GIVEN a replay with a different term or
  payload for an applied index THEN the application reports corruption
  (CR11).

## Send

- SCENARIO Send ambiguity: GIVEN a crash during a send THEN the packet may
  be absent or complete; both outcomes are accepted (CR07).
