# Testing: function references (deep/typed) (S3.2.C1)

*BDD scenarios for the deeper function-reference features (PLAN S3.2
prerequisite for GC): typed `ref.func`/`call_ref`, `ref.as_non_null`,
`br_on_null`, and deep recursion groups. Much of this already decodes/
validates/evaluates today (`call_ref` at `eval.c:1891`); the corpus pins the
typed/function-reference behaviour and any gaps. Stakeholder value: story
0000. Design: `.agent/design/0011-gc.md`.*

## FUN-001 Typed references preserve the function type

SCENARIO: ref.func of a typed function
GIVEN a `ref.func $f` where `$f` has function type `t`
WHEN evaluated
THEN it yields a reference whose function instance has type `t`
AND `call_ref` to it verifies the callee type against the declared
    `(ref $t)` signature before invoking.

SCENARIO: call_ref type checks the operand
GIVEN a `call_ref` whose expected signature is `t`
WHEN the reference operand's type is not `t` (or not a subtype usable at
    that position)
THEN it is rejected by validation (or traps at runtime where null/type
    failure applies), never silently invoked.

## FUN-002 null/type-directed control helpers

SCENARIO: ref.as_non_null and br_on_null on typed references
GIVEN a nullable typed function reference
WHEN `ref.as_non_null` runs on a non-null value it returns the non-null
    value; on null it traps
AND `br_on_null` branches when the value is null and continues otherwise,
    with the type stack narrowed to non-null.

## FUN-003 Deep recursion groups validate

SCENARIO: Mutually recursive typed functions
GIVEN functions whose types live in a recursion group and reference each
    other by type index
WHEN decoded, validated, and evaluated through `call_ref`
THEN recursive calls resolve through the group correctly with no
    false type mismatch.
