# BDD scenarios: recovered application checkpoint

`raft89_config.applied_index` lets the host supply durable knowledge that
entries through index N already reached its application state machine.
Acceptance criteria are in `.agent/acceptance/0010-applied-checkpoint.md`.

SCENARIO AP01 zero preserves v1 semantics
GIVEN config.applied_index == 0
WHEN a node is created over durable history
THEN applied_index == commit_index == 0 and no action is emitted.

SCENARIO AP02 valid recovered checkpoint
GIVEN durable entries 1..5 and applied_index == 3
WHEN a node is created
THEN applied_index == commit_index == 3, last_log_index == 5, and no action
is emitted merely because the checkpoint exists.

SCENARIO AP03 checkpoint at the log end
GIVEN applied_index == last_log_index
WHEN a node is created
THEN creation succeeds and no historical APPLY action is emitted.

SCENARIO AP04 checkpoint beyond the log end
GIVEN applied_index > last_log_index
WHEN a node is created
THEN creation fails with RAFT89_ERR_CORRUPT, `*out` is NULL, and no action
is emitted.

SCENARIO AP05 unverifiable checkpoint
GIVEN applied_index <= last_log_index but the store cannot confirm the entry
WHEN a node is created
THEN creation fails with RAFT89_ERR_STORE and `*out` is NULL.

SCENARIO AP06 no replay below the floor
GIVEN a recovered checkpoint C
WHEN commit advances to C + 2
THEN APPLY is emitted exactly for C + 1 and C + 2, never below C + 1.

SCENARIO AP07 restart repetition
GIVEN a host that persists an application checkpoint after every APPLY
WHEN the node is repeatedly destroyed and recreated
THEN each checkpointed index is applied at most once and the first APPLY
after restart is checkpoint + 1.

SCENARIO AP08 crash before the checkpoint is durable
GIVEN an APPLY at index 4 whose effect became durable but whose checkpoint
did not
WHEN the node restarts with checkpoint 3
THEN APPLY 4 may be emitted again (at-least-once contract).

SCENARIO AP09 role transitions preserve the floor
GIVEN a recovered checkpoint
WHEN elections, step-downs, heartbeats, and rejections occur
THEN applied_index and commit_index never drop below the checkpoint.
