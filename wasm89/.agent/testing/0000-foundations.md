# Testing: foundations

*BDD scenarios for the foundations milestone. Related items are bundled.*

## F-001 Build pipeline produces a warning-free C89 binary

SCENARIO: Building from a clean checkout
GIVEN a clean repository checkout
WHEN `just build` runs
THEN it exits 0
AND it produces `build/wasm89`
AND the compilation used `-std=c89 -pedantic-errors -Wall -Wextra -Werror`
AND the compilation produced no warnings
AND the build fails to produce `build/wasm89` if any source file
    violates C89.

## F-002 Version command

SCENARIO: Querying the CLI version
GIVEN the built `wasm89` binary
WHEN it is invoked as `wasm89 version`
THEN it exits 0
AND it prints the version integer `1`.

SCENARIO: Rejecting unknown usage
GIVEN the built `wasm89` binary
WHEN it is invoked with no arguments or an unknown argument
THEN it exits nonzero
AND it prints a usage message to standard error.

## F-003 C89 strictness is enforced

SCENARIO: Non-C89 constructs are rejected (unacceptable behaviour)
GIVEN a translation unit that uses a `//` comment
WHEN it is compiled with the strict C89 flags
THEN compilation fails with a nonzero exit status.

GIVEN a translation unit that declares a variable after a statement
WHEN it is compiled with the strict C89 flags
THEN compilation fails with a nonzero exit status.

## F-004 Linting

SCENARIO: Shell scripts are lint-clean
GIVEN every shell script in the repository
WHEN `shellcheck -s sh` and `shellcheck -s bash` run against it
THEN both report no findings.

## F-005 Unit tests

SCENARIO: Library unit tests pass
GIVEN the library built with the strict flags
WHEN the unit test binary runs
THEN it exits 0
AND reports that `w89_version` returns `1`.
