# libadt89 — agentic workflow

A green-compliant, nominal algebraic-datatype companion to `libhm89`, written
in strict ISO C89. It is a sibling git repository to `libhm89` (its dependency)
and `green` (the source-profile toolchain used to verify it).

## Hard constraints

- **ISO C89 only**, clean under the green baseline for both GCC and Clang in
  both C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **No HM changes, no HM internals**: `libadt89` includes only the public
  `<hm.h>` and never `hm_internal.h`. It depends on the public type/scheme/
  env/unify API and the three neutral constructor-introspection accessors.
- **No runtime value representation**, no GADTs, no second type system.
- Four-space indentation; functional style; most functions short (2–7 lines).

## Dependency / ownership

`libadt89` depends on `libhm89`; never the reverse.

```
libhm89
   ^
libadt89
   ^
language frontend
```

- `adt_ctx` does NOT own its `hm_ctx`. `adt_ctx_destroy` frees only
  ADT-owned storage. Destroy order is **adt_ctx_destroy before hm_ctx_destroy**.
- ADT metadata uses its own allocator; ADT-generated `hm_type`/`hm_scheme`/
  `hm_env` objects live in the HM arena and are never individually freed by
  ADT code.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` → failing
test → minimum code → refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just smoke`, `just unit`,
`just test`, `just green`, `just check`, `just lint`, `just format`,
`just doctor`, `just clean`.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
