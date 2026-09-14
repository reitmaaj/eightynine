# libsyntax89 — typed ordered syntax DAGs for strict C89

A green-compliant, strict-C89 library for constructing, validating, freezing,
traversing, and transforming typed ordered syntax DAGs. It stores nodes,
ordered structural edges, and source spans; it embeds no language policy.

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **No language policy**: kinds and roles are opaque `unsigned long` values;
  no grammar, schema, name resolution, typing, evaluation, or source text
  ownership lives here.
- **No deletion and no in-place rewrite**: BUILDING admits append-only
  construction; FROZEN admits queries and traversal; transformations build a
  new graph.
- **Failure atomicity**: every operation completes fully or changes nothing
  observable.
- **Bounded stack**: no core operation consumes C call-stack space
  proportional to syntax depth; validation and traversal use explicit stacks.
- Four-space indentation; functional style; worker/controller structure.

## Public contract

- Public header `include/syntax89.h`; names are namespaced `syntax89_*`.
- `syntax89_graph` is caller-owned; its fields are private state.
- `syntax89_add_node`/`add_child`/`set_root` are the only structural mutators
  and are rejected with `SYNTAX89_ESTATE` on a FROZEN graph.
- `syntax89_validate` checks structure only: root presence and validity, root
  indegree, acyclicity, and reachability. Schema checks stay outside.
- `syntax89_freeze` validates then flips the state; it is idempotent.
- Node ids are nonzero, stable for the graph lifetime, and never reused.
- `syntax89_validate`/`syntax89_freeze`/walks allocate O(V) scratch through
  the graph allocator and may return `SYNTAX89_ENOMEM`; queries and child
  iteration allocate nothing.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under
`.agent/acceptance/*.md` cover must-exhibit and must-reject behavior. Coverage
order: one end-to-end smoke test first, then unit tests for every pure
function and non-trivial branch, then model, generated, fault, and stress
testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just model`, `just generated`, `just fault`, `just stress`, `just deep-test`,
`just cpp-check`, `just compile-check`, `just test`, `just matrix`,
`just sanitize`, `just valgrind`, `just coverage`, `just audit`,
`just api-coverage`, `just shell-selftest`, `just coverage-selftest`,
`just build-selftest`, `just green`, `just check`, `just release`,
`just lint`, `just format`, `just clean`.

Scripts are invoked as `sh scripts/<name>`, which ignores the shebang, so
every script carries an explicit `set -eu` in its body; `just shell-selftest`
enforces this. Gates must fail rather than pass vacuously; the coverage,
sanitizer, and build gates carry self-checks.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
