# 0000 - Testing: foundations

*BDD scenarios for the foundations milestone. Related items are bundled.*

## 0000-001 Build pipeline produces a warning-free C89 binary

SCENARIO: Building from a clean checkout
GIVEN a clean repository checkout
WHEN `just build` runs
THEN it exits 0
AND it produces `build/bpf`
AND the compilation used the strict C89 flags
AND the compilation produced no warnings
AND the build fails if any source file violates C89.

## 0000-002 Version command

SCENARIO: Querying the CLI version
GIVEN the built `bpf` binary
WHEN it is invoked as `bpf version`
THEN it exits 0
AND it prints the version integer `1`.

SCENARIO: Rejecting unknown usage
GIVEN the built `bpf` binary
WHEN it is invoked with no arguments or an unknown argument
THEN it exits nonzero
AND it prints a usage message to standard error.

## 0000-003 C89 strictness is enforced

SCENARIO: Non-C89 constructs are rejected (unacceptable behaviour)
GIVEN a translation unit that uses a `//` comment
WHEN it is compiled with the strict C89 flags
THEN compilation fails with a nonzero exit status.

GIVEN a translation unit that declares a variable after a statement
WHEN it is compiled with the strict C89 flags
THEN compilation fails with a nonzero exit status.

## 0000-004 Linting

SCENARIO: Shell scripts are lint-clean
GIVEN every shell script in the repository
WHEN `shellcheck -s sh` and `shellcheck -s bash` run against it
THEN both report no findings.

## 0000-005 Unit tests

SCENARIO: Library unit tests pass
GIVEN the library built with the strict flags
WHEN a unit test binary runs
THEN it exits 0
AND reports that `bpf_version` returns `1`.
