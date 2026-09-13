# Acceptance criteria: allocation failure and leak checks

Traces to `.agent/testing/0009-allocation.md`.

## Must exhibit (exhibit)

- For every allocation index of `raft89_create`, `raft89_propose`, and a
  receive with entries, an injected failure MUST produce
  `RAFT89_ERR_NOMEM` (or the call completes when it does not need that
  allocation) and MUST release every allocation before returning.
- After an injected failure the node MUST remain readable and destroyable.
- `just valgrind` MUST complete with no valgrind errors and no leaks.

## Must reject / fail safely (reject)

- A failed create MUST NOT return a partially constructed node.
- A failed propose MUST NOT append or leave an action outstanding.
- A failed receive MUST NOT emit an action or change the durable store.
