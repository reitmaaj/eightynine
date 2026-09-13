# Testing: ob89 (Obligation-C89) conformance

*BDD scenarios for enforcing the Obligation-C89 normal form over the
wasm89 sources via the `ob89` linter.*

## OBC-001 Sources conform to Obligation-C89

SCENARIO: Every source unit is accepted by ob89
GIVEN each file under `src/*.c`
WHEN the `ob89` linter is run on it (as a whole translation unit)
THEN it reports no findings and exits 0.

SCENARIO: No hidden domain transitions
GIVEN any statement in the sources
WHEN it performs a load, store, call, convert, address-of, pointer
    arithmetic, or branch
THEN that transition is its own statement or dedicated syntactic form and
    is not hidden inside a larger pure expression, branch condition, store
    RHS, call argument, place index, or initializer.
GIVEN a pure arithmetic expression
WHEN it contains only already-computed values (identifiers, constants) and
    the operators `+ - * / % << >> & ^ | < <= > >= == !=` and unary
    `+ - ! ~`
THEN it may nest freely.
GIVEN any expression containing a load, call, cast, address-of, pointer
    arithmetic, assignment, increment, decrement, compound assignment,
    short-circuit operator, conditional operator, comma operator, or
    `sizeof expr`
WHEN it is not the dedicated statement for that obligation class
THEN it is rejected.

## OBC-002 Fixes preserve behavior

SCENARIO: Refactoring to normal form is behavior-preserving
GIVEN a source unit converted to Obligation-C89 normal form
WHEN the full wasm89 test suite (`just test`) is run
THEN every unit, smoke, REPL, and conformance test that passed before the
    conversion still passes.
GIVEN an expression decomposed into witness statements (loads, calls,
    computed indexes, conversions)
WHEN evaluation order or short-circuit control flow is affected
THEN the decomposition reproduces the original order and semantics
    exactly.
