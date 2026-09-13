# libledger89 — agentic workflow

A green-compliant, standalone ISO C89 library for a durable, append-only
sequence of opaque records stored in immutable rotated segments, with crash
recovery and deterministic iteration. It owns the local durability core and
nothing around it: no Raft terms, leaders, replication, SQL, indexes,
queries, capabilities, or application semantics.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Standalone**: libledger89 includes no sibling headers and never links
  another library. libc and POSIX file primitives only. No Raft, query, or
  application code in `include/` or `src/`.
- **Opaque payloads**: the library never interprets record payload bytes.
  `tag` is the only coarse discriminator.
- **No clocks, no policy**: rotation is byte/record-count based or explicit;
  wall-clock rotation belongs to the caller.
- **Explicit durability**: `ledger89_sync()` establishes the crash-durable
  boundary for appends. Structural operations (`rotate`, `truncate_after`,
  `discard_before`) are self-durable and crash-safe step by step.
- Four-space indentation; functional style; most functions short (2-7 lines).

## Public contract

- The public header is `include/ledger89.h`; names are namespaced `ledger89_*`.
- `spec/ledger89-spec.md` is the normative specification; sections map to
  tests under `test/` and scenarios under `.agent/testing/`.
- On-disk format, durability contract, recovery classification, and the
  truncate/discard orderings are fixed in `.agent/design/`.
- Indices are caller-supplied, start at 1, and must be contiguous: the first
  record of an append must equal `last_index + 1`.
- v1 excludes replication, commit decisions, snapshots, query semantics,
  secondary indexes, retention policy beyond explicit `discard_before`, and
  application schemas.

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

The crash fixture table `.agent/testing/0005-crash-fixtures.md` is normative
for the crash suites: every injection point states the pre-state, the durable
filesystem state after the crash, the allowed recovered states, and the
required assertion.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just api`, `just crash`, `just fault`, `just corrupt`, `just model`,
`just adapters`, `just golden`, `just test`, `just long`, `just sanitize`,
`just valgrind`, `just green`, `just check`, `just lint`, `just format`,
`just doctor`, `just bench`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not commit or push
without explicit permission.
