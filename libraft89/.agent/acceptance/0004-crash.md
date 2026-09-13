# Acceptance criteria: crash points

Traces to `.agent/testing/0004-crash.md` and
`.agent/design/0001-crash-contract.md`.

## Must exhibit (exhibit)

- A crash before a durable effect MUST leave the previous durable state
  intact.
- A crash during `HARD_STATE` MUST expose either the old or the complete
  new `(term, vote)` pair, never a mixture.
- A crash during `LOG_APPEND` MUST expose the old log plus any complete
  prefix of the batch, contiguous and uncorrupted.
- A crash during `LOG_TRUNCATE` MUST leave a suffix ending no earlier than
  `first_index - 1`.
- A crash after a completed effect MUST recover the same durable state
  whether or not the action had been acknowledged.
- After restart the node MUST be a follower with `leader_id = NONE`,
  `commit_index = 0`, `applied_index = 0`, and no outstanding action.
- `APPLY` replay MUST carry identical term and payload and MUST be a no-op
  in the application; a mismatch MUST be reported as corruption.
- A crashed send MUST NOT force a definite outcome: the packet may be
  absent or complete.

## Must reject / fail safely (reject)

- No granted vote response may be observable before the corresponding
  `HARD_STATE` acknowledgement.
- No successful `AppendEntries` response may be observable before the
  required truncate and append acknowledgements.
- A torn or mixed hard state MUST NOT be observable after recovery.
- A partially applied command MUST NOT be observable after recovery.
