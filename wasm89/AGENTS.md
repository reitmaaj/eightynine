# Your agentic workflow

## `.agent` directory

Verify that a directory named `.agent` exists in the repository root;
if not, you MUST create it.

All files related to your personal plans and progress MUST go under
this directory.

The project must build and run successfully even if `.agent` directory
is removed.

### Useful files under the `.agent` directory

You SHOULD maintain up-to-date files `.agent/concept/*.md` that describe
the overall concept of the software.

You SHOULD maintain up-to-date files `./agent/acceptance/*.md` containing
acceptance tests for the software under development. Acceptance tests MUST
cover behavior the software MUST exhibit, and MUST also cover behavior that
would be unacceptable. Unacceptable behavior includes behavior that the
software MUST reject, avoid, refuse, fail safely, or report as an error.

You SHOULD maintain up-to-date files `.agent/stories/*.md` that describe
the stakeholders of the software under development and their gained
value in the well-known user story format (AS/I WANT/SO THAT).

You SHOULD maintain up-to-date files `.agent/design/*.md` that describe
e.g the data model, module boundaries, key types, major decisions and
their rationale for the software under development.

You SHOULD maintain up-to-date files `.agent/testing/*.md` that describe
the intended behavior of the software under development in the well-known
BDD scenario format (SCENARIO/GIVEN/WHEN/THEN).

The basename of all the above files MUST start with pattern `NNNN-` where N is
a digit 0-9, followed by a descriptive name for the concern addressed. You
SHOULD bundle multiple logically related items (stories, scenarios, etc.) in a
single file.

### `.agent/tmp`

You MAY maintain all files related to your state that are ephemeral
and/or don't need revision control under `.agent/tmp`.

## Command line tools

You have the well-known GNU `coreutils` at your disposal, as well as
`git` for revision control.

For linting shell scripts, you have the `shellcheck` tool.

For performing structured tasks, you have the `just` tool at your
disposal. You also have the venerable `make` tool available.

Other command line tools such as `grep`, `sed`, `nl`, `find` may also
be available.

### `just` and `Justfile`

In order to perform any actions in the repository besides source file
editing and revision control, you MUST use the `just` command line tool.

You MUST add all such actions to a `Justfile` and execute them
exclusively via `just <action>`; you MUST not run any such actions
directly via shell.

There MUST be only one `Justfile` and it MUST reside in the repository
root. The file MUST be committed to revision control. All invocations of
`just` MUST use that file.

You SHOULD structure the `Justfile` so that it reuses it's own recipes
as appropriate.

### Shell scripts and shell script linting

All shell scripts MUST have a hashbang `#!/bin/sh -eu`. Also, all shell
scripts MUST pass both `shellcheck -s sh` and `shellcheck -s bash`
linting, without any complaints.

## Test-driven development discipline

Every behavior change MUST follow this sequence without exception:

1. Read or write the relevant scenario in `.agent/testing/*.md`.
2. Write or update the relevant acceptance test in `./agent/acceptance/*.md`,
   including at least one unacceptable behavior test when applicable.
3. Write a failing test that encodes that scenario.
4. Write the minimum code that makes the test pass.
5. Refactor with all tests green.

You MUST NOT write implementation code without a corresponding failing
test. You MUST NOT write a test without a corresponding scenario in
`.agent/testing/*.md`; if no scenario exists, you MUST add one first.

For every newly discovered invariant or edge case, you MUST add a test
immediately, even if no defect is present. You SHOULD also add a
corresponding scenario to `.agent/testing/*.md`.

For every newly discovered unacceptable behavior, you MUST add an
acceptance test to `./agent/acceptance/*.md` immediately, even if no defect
is present.

### Coverage order

Before any integration or system testing, you MUST first pass:

* one smoke test proving the feature path runs end-to-end without error;
* unit tests for every pure function and every non-trivial branch.

You MUST NOT proceed to broader testing before smoke and unit tests pass.

## Hypothesis-driven bug fix discipline

Before attempting to fix an identified bug, you MUST inspect the relevant
parts of the repository, any relevant logs and tracebacks and any other
relevant information related to the defect.

Before proceeding to fix the bug, you MUST form several explicit
hypotheses of the root cause of the defect, and you MUST write tests
or other deterministic probes to attempt to rigorously and vigorously
falsify all of them. You MUST NOT attempt to fix the bug assuming a root
cause that has not been subjected to falsification and has been falsified.

You SHOULD assume naively simple root causes for the defects. You SHOULD
NOT assume elaborate root causes unless all simple root cause hypotheses
have been explicitly falsified.

You MUST NOT accept a hypothesis based on confirming information only.
You MAY use confirming information to steer your investigation.

You MUST accept a hypothesis as the assumed root cause of the defect only
if it's the only unfalsified hypothesis left; note that even in this case
it still may not be the true root cause.

### Connection to test-driven development discipline

For every defect for which a root cause hypothesis has survived
falsification, you MUST:

1. Write a failing reproduction test before modifying any code.
2. Attempt to fix the defect.
3. Confirm the reproduction test and the full suite both pass.

You MUST NOT assume the defect has been fixed unless the reproduction
test and the full suite both pass.

## Revision control discipline

You MUST not break the `main` branch: `main` MUST always pass all
testing, without failures, errors, warnings or any indication of defects
or shortcomings.

You SHOULD create a short-lived working branch for each logical
changeset. You SHOULD NOT have more than one such working branch in
progress at a time.

When a working branch has been merged to main, you SHOULD delete that
branch, unless it's prudent to keep it around for a while for some
specific reason.

You MUST NOT, under any circumstances, push to upstream `main` without
asking the user for a permission to do so.

### `.gitignore`

You MUST have only a single, root-level `.gitignore` file in the
worktree.

The `.gitignore` file MUST first ignore everything, then unignore
specific files, directories and filename suffixes.

The `.gitignore` file MUST begin with a header section:

```
*
!.gitignore

!.agent/
!.agent/*.md
!.agent/*/*.md
!.agent/*.sql

!README
!LICENSE

!Justfile
!Makefile

!AGENTS.md
```

All adjustments to the root `.gitignore` MUST go after the header.

## Style rules

You SHOULD write in functional, as opposed to object-oriented, style.
If writing in object-oriented style, you SHOULD compose classes instead
of inheriting.

You SHOULD use an applicable source code formatter, such as `ruff format`,
`gofmt`, `clang-format`, or similar, if available; if not, you SHOULD
ask the user to install one.

You MUST use four spaces for indentation, unless it is invalid to do so.

### Source code factoring

Function, method, class or other source code unit length SHOULD be
inversely proportional to frequency of occurrence.

You SHOULD write most functions (i.e. more than 50%) to be 2-3 lines
long; this implies a strict discipline to write many small helpers.
Functions longer than 7 lines should already be rare.

The rationale of this heuristic is to facilitate unit testing; this
rationale also implies that functions SHOULD be pure and not have free
variables.

## Agent execution

When the user says "proceed", "implement", "go ahead", or equivalent:

* you MUST execute at least one concrete tool or action before replying;
* you MUST report completed work, not intent to perform work;
* continue autonomously until blocked by a real missing requirement.

If blocked, you MUST report:

* the exact blocker;
* evidence, including command output or error text;
* one proposed unblocking action.

Before replying to the user, you MUST verify that:

* a concrete action ran this turn;
* project state advanced measurably;
* your reply reports results, not intentions.

