# BDD scenarios: batched proposals

`raft89_proposev()` proposes one consecutive batch of application commands
that becomes one local log-append action. Acceptance criteria are in
`.agent/acceptance/0011-proposev.md`.

SCENARIO PV01 basic batch
GIVEN a leader and three commands
WHEN raft89_proposev runs
THEN exactly one RAFT89_ACT_LOG_APPEND carries three consecutive entries
with the leader's term and the exact payloads, and first_index names the
first entry.

SCENARIO PV02 empty or null batch
GIVEN count == 0 or commands == NULL with count > 0
WHEN raft89_proposev runs
THEN it refuses with no log mutation, no action, and no index reservation.

SCENARIO PV03 payload validation
GIVEN a command with size > 0 and data == NULL
WHEN raft89_proposev runs
THEN the entire call fails before any entry becomes visible.

SCENARIO PV04 entry-count limit
GIVEN count == max_append_entries and count == max_append_entries + 1
WHEN raft89_proposev runs
THEN the first is accepted and the second is rejected atomically.

SCENARIO PV05 byte limit
GIVEN payload sums of max_append_bytes - 1, max_append_bytes, and
max_append_bytes + 1
WHEN raft89_proposev runs
THEN only the first two are accepted.

SCENARIO PV06 sum overflow
GIVEN command sizes whose saturating sum overflows raft89_size
WHEN raft89_proposev runs
THEN it is rejected as a limit error; wraparound never makes an oversized
batch appear small.

SCENARIO PV07 index overflow
GIVEN last_log_index such that the batch would pass 2^64-1
WHEN raft89_proposev runs
THEN the node faults and RAFT89_ERR_LIMIT is returned without an action.

SCENARIO PV08 role and action state
GIVEN a follower, or a leader with an outstanding action
WHEN raft89_proposev runs
THEN RAFT89_NOT_LEADER or RAFT89_BUSY is returned with no mutation.

SCENARIO PV09 single-node commit order
GIVEN a single-node cluster
WHEN a three-entry batch is appended and acknowledged
THEN the actions are LOG_APPEND [1,2,3], APPLY 1, APPLY 2, APPLY 3 in order.

SCENARIO PV10 multi-node replication
GIVEN a three-node cluster and a three-entry batch
WHEN the local append is acknowledged
THEN the replication SEND carries all three entries with their indices.

SCENARIO PV11 equivalence with propose
GIVEN one command X
WHEN raft89_propose(X) and raft89_proposev([X]) run on otherwise identical
nodes
THEN the observable action sequence and indices are equivalent.

SCENARIO PV12 allocation failure
GIVEN a deterministic allocation failure at any allocation point
WHEN raft89_proposev runs
THEN it returns an error with no partial batch, no action, and no consumed
index.
