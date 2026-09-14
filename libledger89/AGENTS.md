# libledger89 — agentic workflow

A green-compliant, standalone ISO C89 library for a durable, ordered,
rewindable sequence of opaque byte records addressed by stable logical
positions. It owns the local durability core and nothing around it: no Raft
terms, leaders, replication, message queues, SQL, indexes, queries,
capabilities, or application semantics.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Standalone**: libledger89 includes no sibling headers and never links
  another library. libc and POSIX file primitives only. No Raft, query, or
  application code in `include/` or `src/`.
- **Opaque payloads**: the library never interprets record payload bytes.
  There is no record tag, type, or timestamp.
- **Ledger-assigned positions**: callers cannot choose indices. Optional
  compare-and-append (`appendv_at`) compares revision and end.
- **Explicit durability**: `ledger89_sync()` writes a stable marker and
  advances `stable_end`, the exact local crash-recovery frontier.
- **No clocks, no policy**: rotation is explicit or at a fixed internal byte
  target; retention policy is the caller's; the only entropy is the internal
  I/O `entropy` hook used for ledger identity.
- Four-space indentation; functional style; most functions short (2-7 lines).

## Public contract

- The public header is `include/ledger89.h`; names are namespaced `ledger89_*`.
- `spec/ledger89-spec.md` is the normative specification; sections map to
  tests under `test/` and scenarios under `.agent/testing/`.
- On-disk format v2, durability, recovery, and the Raft adapter boundary are
  fixed in `.agent/design/`.
- Indices are ledger-assigned contiguous positions starting at 1. `first`,
  `stable_end`, and `end` form the three-boundary model; suffix truncation
  increments `revision` because indices may be reused.
- v2 excludes transactions, WAL/MQ semantics, topics, acks, subscriptions,
  consumer groups, SQL/query semantics, schemas, secondary indexes,
  replication, consensus, membership, networking, threads, IPC, snapshots,
  retention policy, timestamps, application record types, and serialization.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then crash, fault, corruption, and model-based testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just api`, `just adapters`, `just crash`, `just fault`, `just fault-nomem`,
`just fault-eintr`, `just corrupt`, `just model`, `just stress`,
`just golden`, `just golden-gen`, `just test`, `just long`, `just bench`,
`just sanitize`, `just valgrind`, `just build-shared`, `just strictrun`,
`just gen-strict-tables`, `just strict-fast`, `just strict`,
`just strict-long`, `just strict-sanitize`, `just strict-valgrind`,
`just ratio`, `just coverage`, `just coverage-report`, `just green`,
`just green-fix`, `just check`, `just lint`, `just format`, `just doctor`,
`just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not commit or push
without explicit permission.
