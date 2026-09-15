# Acceptance: AppendEntries conflict hints

## Mandatory behaviors (must exhibit)

- A successful response carries `conflict_term == NONE` and
  `conflict_index == NONE`.
- A rejection because `prev_log_index` is absent reports
  `conflict_term == NONE` and `conflict_index == follower_last_log_index + 1`.
- A rejection because `prev_log_term` mismatches reports the follower term
  at `prev_log_index` and the first local index carrying that term.
- A leader that contains the conflict term jumps `next_index` to the last
  index carrying it plus one; a leader that lacks it jumps to
  `conflict_index`; either result is clamped to `[1, last_log_index + 1]`.
- Conflict hints change only the number and contents of intermediate
  messages; committed-log semantics are unchanged.
- Hint values above 2^32 survive without truncation.

## Unacceptable behaviors (must reject / refuse)

- A successful response carrying a non-NONE conflict field MUST be rejected
  with `RAFT89_ERR_PROTOCOL`.
- A failed response whose `match_index` is not NONE, or whose
  `conflict_index` is NONE, MUST be rejected with `RAFT89_ERR_PROTOCOL`.
- A stale-term rejection MUST NOT leave the response without a valid hint
  pair.
- A higher-term rejection MUST still trigger the normal term transition;
  conflict hints MUST NOT override term handling.
- A malformed hint MUST NOT change `next_index`, role, or any other state.
