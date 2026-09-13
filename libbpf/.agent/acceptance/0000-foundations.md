# 0000 - Acceptance: foundations

*Acceptance criteria for the foundations milestone. Each item is either a
behaviour the software MUST exhibit or a behaviour it MUST reject.*

## MUST

* `just build` exits 0 and produces `build/bpf`.
* The build uses `-std=c89 -pedantic-errors -Wall -Wextra -Werror`
  (`-Werror` turns any warning into a build failure, so a successful build is
  a warning-free build).
* `bpf version` exits 0 and prints `1`.
* `bpf` with no arguments or an unknown argument exits nonzero and prints a
  usage message.
* `just lint` runs `shellcheck -s sh` and `shellcheck -s bash` on every
  repository shell script; a finding fails the recipe.
* The C unit test for `bpf_version` passes (returns `1`).
* A translation unit using a `//` comment, compiled with the strict C89
  flags, fails to compile (probe test must pass, i.e. compilation MUST fail).
* A translation unit declaring a variable after a statement, compiled with
  the strict C89 flags, fails to compile.

## MUST NOT

* The build MUST NOT succeed with any compiler warning.
* The build MUST NOT succeed if any translation unit violates C89.
* A shell script MUST NOT pass linting if it lacks a `#!/bin/sh -eu`
  hashbang or triggers shellcheck findings.
* `bpf` MUST NOT exit 0 for unknown or missing arguments.
