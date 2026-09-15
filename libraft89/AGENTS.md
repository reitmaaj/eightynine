# libraft89 — agentic workflow

A green-compliant, standalone C89 library for the deterministic Raft
consensus protocol. It owns leader election, log replication, commit
calculation, and ordered application, and nothing around it. It is a sibling
git repository to `libfx89`, `libadt89`, and `librepl89`, and depends on no
sibling library.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Standalone**: `libraft89` includes no sibling headers and never links
  another library. No sockets, threads, filesystem, serialization, event
  loop, RPC framework, or database code.
- **No host policy**: storage is read through `raft89_store` callbacks;
  persistence, sending, and application happen through acknowledged
  `raft89_action` values. Exactly one action may be outstanding.
- **No wall clock, no ambient randomness**: logical time enters through
  `raft89_tick`; randomness through the configured `raft89_random` source.
  Equal inputs plus an equal event sequence yield identical actions.
- Four-space indentation; functional style; most functions short (2-7 lines).

## Public contract

- The public header is `include/raft89.h`; names are namespaced `raft89_*`.
- `spec/raft89-spec.md` is the normative specification; sections map to tests
  under `test/` and scenarios under `.agent/testing/`.
- Persistent state is `current_term`, `voted_for`, and the log. Everything
  else (role, `leader_id`, `commit_index`, `applied_index`, timers,
  replication state) is volatile and is rebuilt on restart.
- v1 excludes dynamic membership, snapshots, pre-vote, leadership transfer,
  ReadIndex/leases, transport, wire encoding, threads, clocks, and storage
  integration.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then crash, simulation, and exhaustive testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just crash`, `just sim`, `just test`, `just exhaustive`, `just sanitize`,
`just build32`, `just test32`, `just sanitize32`, `just valgrind32`
(best-effort ILP32; GCC default, `CC=clang` selects clang; skip cleanly
without multilib or the 32-bit runtimes), `just green`,
`just check`, `just lint`, `just format`, `just doctor`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
