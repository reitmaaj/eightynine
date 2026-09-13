# Acceptance: function references (deep/typed) (S3.2.C1)

*Relates to `.agent/testing/0030-funcref.md`. Stakeholder value: story 0000.
Design: `.agent/design/0011-gc.md`. Cross-reference: reference-op
conformance already covered under `0009`/`0010` for the existing subset.*

## MUST

* `ref.func $f` MUST yield a reference whose function instance carries the
  declared type `t`.
* `call_ref` MUST verify the callee reference's type against the expected
  signature before invoking; a type mismatch MUST be rejected (validation)
  or trapped (runtime), never silently invoked.
* `ref.as_non_null` MUST return the non-null reference when non-null and
  trap when the operand is null.
* `br_on_null` MUST branch when the operand is null and continue (with the
  stack narrowed to non-null) otherwise.
* Functions whose types form a recursion group MUST decode, validate, and
  evaluate through `call_ref` with recursive calls resolving correctly.

## MUST NOT

* A typed reference MUST NOT be invoked under a signature it does not
  satisfy, or be accepted where a different function type is required.
* `ref.as_non_null`/`br_on_null`/`call_ref` MUST NOT mis-handle null
  (e.g. as_non_null on null MUST trap; call_ref on null MUST trap).
* Deep recursion-group function types MUST NOT be flattened or mistyped so
  that mutual recursion fails validation.

## Gate

* The reference/function-reference conformance subset
  (`call_ref`, `ref*`, `br_on_null`/`br_on_non_null`, `return_call_ref`)
  green; `just lint ob89 test` green.
