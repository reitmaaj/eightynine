# BDD scenarios: portable 64-bit term and index scalars

libraft89 v2 represents terms and indices as portable 64-bit values: a public
`{hi, lo}` structure with private native arithmetic. Acceptance criteria are
in `.agent/acceptance/0009-portable-scalars.md`.

## Scalars

SCENARIO U01 zero and construction
GIVEN raft89_u64_zero and raft89_u64_from_u32
WHEN each result is inspected
THEN zero is {0, 0} and from_u32 yields hi == 0, lo == value.

SCENARIO U02 comparison matrix
GIVEN the boundary values 0, 1, 2^32-1, 2^32, 2^32+1, 2^63-1, and 2^64-1
WHEN raft89_u64_cmp and raft89_u64_equal run on every ordered pair
THEN the comparison sign matches mathematical order and equality is exact.

SCENARIO U03 state above 2^32
GIVEN durable hard state and log metadata with term and index above 2^32
WHEN a node is created and raft89_status_get runs
THEN every high and low word is preserved.

SCENARIO U04 election across 2^32
GIVEN current_term == 2^32-1
WHEN an election starts
THEN the new term is exactly 2^32, without truncation or wrap.

SCENARIO U05 term overflow
GIVEN current_term == 2^64-1
WHEN an election starts
THEN RAFT89_ERR_LIMIT is returned and no state changes.

SCENARIO U06 index overflow
GIVEN last_log_index == 2^64-1
WHEN raft89_propose runs
THEN the node faults, RAFT89_ERR_LIMIT is returned, and no action appears.

SCENARIO U07 64-bit protocol traffic
GIVEN terms and indices around 2^32
WHEN RequestVote, AppendEntries, responses, actions, and status are exchanged
THEN every value round-trips exactly and no low-word truncation occurs.
