# BDD scenarios: AppendEntries conflict hints

A failed AppendEntries response carries hints that let a leader jump
`next_index` instead of decrementing one position per round. Acceptance
criteria are in `.agent/acceptance/0012-conflict-hints.md`.

SCENARIO CH01 successful response
GIVEN a successful AppendEntries exchange
WHEN the follower replies
THEN success is 1, match_index is the highest match, and both conflict
fields are NONE.

SCENARIO CH02 follower log too short
GIVEN a follower whose log ends before prev_log_index
WHEN the follower rejects the prefix
THEN conflict_term is NONE and conflict_index is last_log_index + 1.

SCENARIO CH03 term mismatch
GIVEN a follower whose term at prev_log_index differs from prev_log_term
WHEN the follower rejects the prefix
THEN conflict_term is that local term and conflict_index is the first
local index carrying it.

SCENARIO CH04 conflict run starts at index 1
GIVEN a long run of one term beginning at index 1
WHEN a mismatched prefix is rejected
THEN conflict_index is 1.

SCENARIO CH05 stale-term rejection
GIVEN an AppendEntries with a stale term
WHEN the follower replies
THEN the response still carries a valid conflict hint pair.

SCENARIO CH06 leader has the conflict term
GIVEN a leader whose log contains conflict_term
WHEN a rejection arrives
THEN next_index jumps to the last leader index with that term + 1.

SCENARIO CH07 leader lacks the conflict term
GIVEN a leader whose log does not contain conflict_term
WHEN a rejection arrives
THEN next_index becomes conflict_index.

SCENARIO CH08 hint past the log end
GIVEN a conflict_index above last_log_index + 1
WHEN a rejection arrives
THEN next_index is clamped to last_log_index + 1.

SCENARIO CH09 malformed hints
GIVEN a success response with conflict hints, or a failure response with
no usable conflict index, or a failure with a nonzero match_index
WHEN received
THEN RAFT89_ERR_PROTOCOL is returned with no state change.

SCENARIO CH10 higher-term rejection
GIVEN a rejection carrying a higher term
WHEN received
THEN the normal higher-term step-down occurs and hints never override it.

SCENARIO CH11 64-bit hints
GIVEN a conflict index above 2^32
WHEN a rejection arrives
THEN next_index preserves the full 64-bit value without low-word
truncation.
