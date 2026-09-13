# Acceptance: iterative machine — structured control flow (S2.2)

*Relates to `.agent/testing/0020-iter-blocks.md`. Stakeholder value: story
0006. Design: `.agent/design/0007` §2, §8. Legacy recursive stepper stays
the default and bit-identical until S2.6.*

## MUST

* The iterative driver MUST step the top `BLOCK`/`FUNC` level in place,
  dispatching migrated opcodes directly (no C-recursion per nesting level)
  and advancing `pc`.
* `block`/`loop`/`if` MUST push one `BLOCK` level carrying the correct
  `src`/`nsrc`, `pc`, `end` (arm end for `if`/`else`), `contpos`/`contn`,
  `exit_arity`, and value-stack `base`.
* `if` MUST evaluate its condition and select then/else arm correctly.
* On normal completion a level MUST be popped and MUST leave exactly
  `exit_arity` results above its `base` in the caller's frame, preserving
  order (multi-value).
* `br`/`br_if`/`br_table` MUST pop levels down to the target label, splice
  the branch's results above the target `base`, and resume at the target's
  continuation; a `loop` target MUST re-enter its continuation.
* `br_table` MUST use the selector's target when in range and the default
  target when out of range.
* A `br` reaching an enclosing function frame MUST behave as a return.
* Every migrated program under `W89_ITER=1` MUST produce results
  bit-identical to the legacy default on the same program.
* The `block`/`if`/`loop`/`br`/`br_table` conformance subset MUST be green
  under `W89_ITER=1` with outcomes identical to the legacy default.
* Any not-yet-migrated instruction in a body MUST fall back cleanly to the
  legacy path without corrupting the level stack.

## MUST NOT

* The iterative driver MUST NOT change evaluation results, trap behaviour,
  exception propagation, or multi-value splice order relative to legacy.
* It MUST NOT pop past a level base when splicing branch results, or leave
  more/fewer than `exit_arity` values on a level's normal exit.
* A `br`/`br_table` MUST NOT target a label that is not open, or beyond the
  enclosing function frame, without trapping as the reference does.
* The driver MUST NOT charge the exhaustion budget for `block`/`loop`/`if`
  nesting (only non-tail `FUNC` frames are charged).
* Migrated control flow MUST NOT C-recurse: at no nesting depth may a block
  be stepped by recursing into `step_label`/`step_frame`.
* The rewrite MUST NOT regress the legacy default path: `just lint ob89
  test` MUST stay green with no change to default (`W89_ITER` unset)
  results.

## Gate

* `just lint ob89 test` green with legacy default unchanged.
* `block`/`if`/`loop`/`br`/`br_table` conformance subset green under both
  `W89_ITER=1` and the default, with identical outcomes.
* The internal two-driver differential (same .wast, both drivers) MUST show
  zero mismatch.
