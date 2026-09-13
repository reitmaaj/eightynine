# Testing: iterative machine — structured control flow (S2.2)

*BDD scenarios for migrating `block`/`loop`/`if`/`br`/`br_if`/`br_table`
onto explicit `BLOCK` machine-stack levels (PLAN S2.2). Stakeholder value:
story 0006. Design: `.agent/design/0007` §2, §8. Legacy recursive stepper
remains the default and must be bit-identical; the iterative driver is
selected by `W89_ITER=1`.*

## ITB-001 The iterative driver steps the top level in place

SCENARIO: Top-of-stack dispatch over a migrated body
GIVEN a config with one open `BLOCK` level over an instruction range that
    contains only migrated opcodes (plain, const, numeric, compare,
    convert, local, global, drop, select, unreachable, block, loop, if,
    br, br_if, br_table)
WHEN the driver `w89_eval_iter` advances that level's pc
THEN each migrated opcode executes directly and the pc advances without
    C-recursing into the legacy stepper
AND any not-yet-migrated opcode (call, memory/table, exceptions, tail)
    produces a needs-legacy signal so the function re-enters `w89_eval`.

SCENARIO: Migrated program equals legacy result
GIVEN the same function body
WHEN it is run under `W89_ITER=1` and under the legacy default
THEN both produce the identical result values and status (bit-identical).

## ITB-002 `block`/`loop`/`if` push and pop a `BLOCK` level

SCENARIO: Structured entry pushes a level with correct extent
GIVEN a `block`/`loop`/`if` instruction with a known blocktype
WHEN the driver enters it
THEN it pushes one `BLOCK` level recording `src`/`nsrc`, `pc`, `end`
    (arm end for `if`/`else`), `contpos`/`contn`, `exit_arity` and a
    value-stack `base` equal to the current unified `vsn`.

SCENARIO: `if` evaluates its condition and chooses an arm
GIVEN an `if` with `else`
WHEN the condition value is non-zero
THEN the then-arm runs
AND when the condition is zero the else-arm runs
AND both produce the same result as the legacy stepper.

SCENARIO: Normal completion splices multi-value results
GIVEN a `block` whose body leaves `exit_arity` values above its `base`
WHEN the level completes normally
THEN it is popped and exactly `exit_arity` results remain above `base` in
    the caller's value frame
AND nested multi-value `block` results are preserved in order.

## ITB-003 `br`/`br_if`/`br_table` pop to the target label

SCENARIO: Unconditional branch to an enclosing label
GIVEN a depth-k `br` whose target is an open `BLOCK`
WHEN the branch executes
THEN levels are popped down to (but not including) the target and the
    branch results are spliced above the target's `base`
AND execution resumes at the target's continuation position.

SCENARIO: Branch to a loop re-enters its continuation
GIVEN a `br` whose target is a `loop`
WHEN the branch executes
THEN the level resumes at the loop's `contpos`/`contn` and re-executes the
    body.

SCENARIO: `br_table` selects by index, default out of bounds
GIVEN a `br_table` with k targets
WHEN the selector is less than k
THEN the corresponding target is branched to
AND when the selector is out of range the default target is branched to.

SCENARIO: Branch to a function frame is a return
GIVEN a `br` whose target label count reaches the enclosing function frame
WHEN the branch executes
THEN it behaves as a function return, popping the frame and splicing its
    results to the caller.

SCENARIO: Branch honour of arity and effect order
GIVEN branches that carry a non-empty result arity out of nested blocks
WHEN they fire
THEN the exact `exit_arity` values (and their order) are preserved above the
    target base, matching the legacy stepper.

## ITB-004 Internal differential of the two drivers

SCENARIO: Conformance block files agree across drivers
GIVEN each `block`/`if`/`loop`/`br`/`br_table` conformance .wast file
WHEN run under the legacy default and under `W89_ITER=1`
THEN every pass/trap/exhaustion/exception outcome is identical.

## ITB-005 Exhaustion budget unaffected by blocks

SCENARIO: Blocks are uncharged against the budget
GIVEN a function body containing only deeply nested blocks and branches
    (no non-tail calls)
WHEN it is run under the iterative driver
THEN it never triggers exhaustion from block nesting alone
AND the budget still charges only open non-tail function frames.
