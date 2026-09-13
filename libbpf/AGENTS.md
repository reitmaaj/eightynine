# libbpf - agentic workflow

`libbpf` is a BPF Instruction Set Architecture (ISA) interpreter/VM in strict
ISO C89 as a sibling git repository, implemented from RFC 9669
(`tmp/rfc9669.txt`). It decodes, validates, and executes eBPF programs on a
register machine.

## Hard constraints

- **green profile** (`just green`): `src/` must compile as strict C89 and
  strict C23 under GCC and Clang, pass the green semantic checks, and use the
  canonical Allman format. The green gate is currently RED: the sources were
  authored under an earlier, less strict profile and still carry
  green-hidden-control, green-flat, green-null and related findings.
- No C99 features in `src/`: no `//` comments, no mixed declarations, no
  `for (int i = ...)`, no `stdbool`, no `inline`, no `__attribute__`.
- Four-space indentation, Allman braces, 80 columns.
- Conformance groups supported: `base32`, `base64`, `divmul32`, `divmul64`,
  `atomic32`, `atomic64`. The deprecated `packet` group and the host-specific
  64-bit-immediate subtypes (`map_fd`/`map_val`/`var_addr`/`code_addr`/
  `map_idx`/`map_val_idx`) are rejected as unsupported.

## `just` and `make`

Use `just` for all actions. `just build` compiles the `bpf` CLI (library
sources + main), `just test` runs the suite (smoke, unit, e2e), `just check`
runs the green gate, `just lint` runs shellcheck and
clang-format, `just memcheck` runs valgrind memcheck. All shell scripts pass
`shellcheck -s sh` and `shellcheck -s bash`.

## `.agent`

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/*` with `NNNN-` names. The project must build and run
without `.agent`.

## TDD

Every behavior change follows: scenario in `.agent/testing/*.md` -> failing
test -> minimum code -> refactor. Acceptance tests cover must-exhibit and
must-reject behavior. Coverage order: one end-to-end smoke test first, then
unit tests for every pure function and non-trivial branch, then broader
integration tests.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
