# Acceptance criteria: follower AppendEntries, commit, apply

Traces to `.agent/testing/0002-follower-append.md`.

## Must exhibit (exhibit)

- The consistency check MUST accept `prev_log_index = 0` with
  `prev_log_term = 0`, MUST accept an existing entry with a matching term,
  and MUST fail when the index is beyond the local log or the term differs.
- A failed consistency check MUST leave the durable log unchanged, MUST
  emit no log action, and MUST reply failure with `match_index = 0`.
- A matching RPC MUST append only the missing suffix and MUST NOT rewrite
  entries that already match.
- A conflict MUST emit `LOG_TRUNCATE(first_divergent)` before
  `LOG_APPEND(replacement)`, and the successful response MUST be emitted
  only after both effects are acknowledged.
- Truncation MUST never target an index at or below `commit_index`.
- `commit_index` MUST advance to `min(leader_commit, matched_index,
  last_log_index)` and MUST never decrease.
- Committed entries MUST be applied strictly in increasing index order with
  exactly one `APPLY` outstanding at a time.
- Each `APPLY` MUST carry the term, index, and payload of the durable log
  entry, and the payload MUST remain valid until acknowledgement.
- A `FATAL` result on `APPLY` MUST fault the node.

## Must reject / fail safely (reject)

- `entry_count` above `max_append_entries`, aggregate payload bytes above
  `max_append_bytes`, `entries == NULL` with a non-zero count,
  non-consecutive or misaligned indices, zero term, zero index, or a null
  payload pointer with non-zero size MUST return `RAFT89_ERR_PROTOCOL` with
  no state change and no action.
- A stale-term AppendEntries MUST be refused with the current term and MUST
  not modify the log, role, or leader.
- An equal-term AppendEntries arriving at a leader MUST be rejected as a
  protocol error rather than silently accepted.
- A storage failure while checking the prefix or loading an entry MUST
  fault the node rather than emit a partial action.
