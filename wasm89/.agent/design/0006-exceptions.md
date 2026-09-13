# Design: exceptions (Phase D)

*Design notes for the Phase D milestone: the evaluator's `throw`,
`throw_ref`, and `try_table` handling, the `Handler`/`Throwing`
administrative instructions, exception ownership, the REPL/driver
`assert_exception` protocol, and the import-matching fixes surfaced by the
conformance driver.*

## Handler / Throwing administrative instructions

`try_table` desugars exactly like the reference interpreter's
`TryTable` case: the body is wrapped in a forward `Label` (block
semantics: `br 0` targets the try_table's own end), and that label is
wrapped in a `Handler` that borrows the decoded catch clauses
(`w89_ainstr.catches`/`ncatches` point into the module's flat
instruction vector; the module outlives evaluation via the store).

`Handler` stepping (`step_handler`, a port of `eval.ml`'s `Handler`
cases):

* body empty -> merge the handler's value stack into the enclosing
  stack, consume the handler, then consume its `skip` (the try_table's
  body range plus its `end` opcode);
* `Throwing` at body head -> walk the catch clauses in declared order:
  - `catch` on tag-instance pointer identity -> append the payload to the
    enclosing stack and `step_br` to the clause label;
  - `catch_ref` -> same, plus a heap `w89_exn` (store-owned) wrapping the
    payload and an `exnref` value pushed on top;
  - `catch_all` -> `step_br` with no payload;
  - `catch_all_ref` -> `w89_exn` + `exnref` only;
  - no match -> the handler becomes the `Throwing` (hoisted outward);
* jumping head -> hoist (breaks/returns/traps pass through untouched);
* otherwise -> step into the body as a sub-configuration.

Tag-instance identity (`a == inst->tags[x]` against the *current* frame)
is what makes imported tags match: an import aliases the exporting
module's tag instance, so the catch resolves to the same pointer.

## Exceptions at the top level

A `Throwing` that propagates to the configuration head ends the
invocation with `W89_EVAL_EXCEPTION`, carrying the tag instance and the
payload (`w89_eval` already transferred `vs0` into `w89_eval_out`).
`throw` pops the tag's parameter count from the operand stack;
`throw_ref` on a null reference traps with "null exception reference"
and otherwise rethrows the referenced exception's tag and payload (a
fresh copy, since the `w89_exn` stays owned by the store).

## Exception ownership

`w89_exn` objects are allocated by `w89_exn_alloc`, registered in the
store's `exns` array (which takes ownership of the payload transferred
from the throwing instruction), and freed in `w89_store_free`. This
matches the store-owned-instance pattern used for `funcinst`,
`taginst`, etc. A `catch_ref`/`catch_all_ref` match transfers the
`Throwing`'s payload into the new `w89_exn` (nulling the throwing
instruction) before shallow-copying the values onto the operand stack.

## REPL / driver protocol

* `invoke` answers `@exception` when an invocation ends with
  `W89_EVAL_EXCEPTION`, distinct from `@return`, `@trap`, and
  `@exhaustion`.
* New `assert_exception <name> <func> <nargs> <arg>*` command answers
  `@pass` iff the invocation throws; failure messages use a per-status
  name (`exception`/`trap`/`exhaustion`) instead of a generic default.
* `assert_return` matches an anonymous `(ref.func)` expectation
  (`ref.func:*` token) against any non-null function reference.
* The driver processes `assert_exception` commands, and `module_check`
  now counts an `assert_invalid`/`assert_malformed`/`assert_unlinkable`
  module that loads successfully as a failure rather than a pass.

## Import-matching corrections (surfaced by the driver fix)

Fixing the driver's `@ok`-counts-as-passed bug exposed latent matching
defects, now corrected to match the reference `Match` module:

* abstract heaptype subtyping: `none` is only a subtype of the `eq`
  family (`none <: t` iff `t <: any`), not of `func`/`extern`/`exn` or
  the `no*` types; `nofunc`/`noextern`/`noexn` are only subtypes of
  their own family;
* `w89_match_externtype`:
  - tables match their element reference types invariantly
    (`actual <: expected` and `expected <: actual`), since tables are
    mutable;
  - immutable globals match `actual <: expected`; mutable globals
    invariantly (unchanged);
  - functions match by full deftype (`w89_match_deftype_x`, which
    checks finality and walks supertypes) rather than a bare structural
    function-type match;
  - tags match deftypes invariantly (both directions).
  To support this, `w89_externtype` gained a `typeidx` and `w89_taginst`
  gained `env`/`typeidx`.
