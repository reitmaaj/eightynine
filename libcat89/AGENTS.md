# libcat89 — agentic workflow

`libcat89` is a small, representation-neutral generic category-theory substrate
for strict ISO C89: a minimal category core plus strictly layered categorical
structures and orthogonal computational capabilities. It is a self-contained
sibling git repository (no external dependency).

## Hard constraints

- **ISO C89 only**, clean under the green baseline for GCC and Clang in both
  C89 and C23 modes, plus the green clang-tidy semantic checks and the
  canonical Allman format (`.clang-format`). Verified via the sibling `green`
  driver (`just green` / `just check`).
- **Minimal category core**: `core.h` exposes only dom/cod/identity/compose
  (plus structural object identity and morphism lifetime). It must include no
  derived structure and carry no hidden equality/enumeration/finiteness/
  execution dependency.
- **Derived structures are external wrappers**: isomorphisms, split morphisms,
  limits, functors, natural transformations, and generic category constructions
  package ordinary `cat89_mor`/`cat89_obj` values and never decorate or subclass
  them. No first-class derived structure may modify the core ABI.
- **Capabilities stay orthogonal**: equality, enumeration, hashing, finiteness
  are optional; their absence never alters the category ABI or disables core
  operation.
- **No implicit collapse**: no generic code identifies equal/isomorphic objects
  or equivalent categories by pointer identity.
- **One status domain** (`cat89_status`); callbacks return `cat89_status`;
  statuses propagate verbatim (`CAT89_NOMEM` is never rewritten).
- **One ownership convention**: `cat89_mor **out` returns one owned reference;
  `const cat89_mor *` / `const cat89_obj *` return borrowed references.
- Four-space indentation; functional style; most functions short (2-7 lines).
- The public layout is a nested header tree under `include/cat89/`.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing.

## `just` and `make`

Use `just` for all actions: `just build`, `just headers`, `just smoke`,
`just unit`, `just test`, `just green`, `just check`, `just baseline`,
`just lint`, `just format`, `just doctor`, `just clean`. `make` delegates.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
