# Acceptance criteria: reference host loop

Traces to `.agent/testing/0010-host-example.md`.

## Must exhibit (exhibit)

- The example MUST compile as strict ISO C89 and link only the public
  library.
- The drain loop MUST perform every durable effect before acknowledging its
  action and MUST capture sends in the loopback queue.
- The example MUST become leader, append, commit, and apply the proposed
  command, printing exactly one applied line.
- The example MUST exit zero on success.

## Must reject / fail safely (reject)

- Any unexpected role, receive, or action result MUST print an error and
  exit nonzero.
