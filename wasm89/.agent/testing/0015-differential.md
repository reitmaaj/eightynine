# Testing: cross-engine differential harness

*BDD scenarios for the differential tester (design `0008`). The harness
runs identical generated programs under wasm89 and reference engines and
requires agreement.*

## DIFF-001 Single-case agreement

SCENARIO: Engines agree on a value
GIVEN a generated module and entrypoint with a fixed argument vector
WHEN it is run under wasm89 and under a reference engine
THEN both return ok with equal result vectors, reported as PASS.

## DIFF-002 Exact finite float equality

SCENARIO: Finite floats match bit-for-bit
GIVEN a float-returning entrypoint whose result is finite
WHEN compared across engines
THEN the results are equal only if the raw bit patterns are identical,
    reported as PASS; a differing finite bit pattern is reported FAIL.

## DIFF-003 NaN rule

SCENARIO: NaN results compare equal regardless of payload
GIVEN a float-returning entrypoint whose result is NaN
WHEN one engine returns any NaN payload and the other returns a (possibly
    different) NaN payload or canonical NaN
THEN the case is reported PASS.
SCENARIO: NaN versus finite is a mismatch
GIVEN a case where one engine returns NaN and the other returns a finite
    value
WHEN compared
THEN the case is reported FAIL.

## DIFF-004 Trap and exhaustion agreement

SCENARIO: Both engines trap
GIVEN a case where wasm89 traps
WHEN the reference engine also traps
THEN the case is reported PASS (trap messages are not compared).
SCENARIO: Status disagreement
GIVEN a case where wasm89 returns ok but the reference engine traps
    (or vice versa)
WHEN compared
THEN the case is reported FAIL.

## DIFF-005 Missing engine is an error

SCENARIO: Engine absent
GIVEN a configured reference engine that is not installed on PATH
WHEN the sweep runs
THEN it reports a clear EngineMissing error and a nonzero exit, never a
    silent PASS.

## DIFF-006 Harness can fail (comparator honesty)

SCENARIO: Forced mismatch detected
GIVEN a stub oracle that returns a result differing from wasm89's for the
    same case
WHEN the self-test runs
THEN the harness reports FAIL for that case, proving the comparator does
    not degenerate to "always pass".

## DIFF-007 Determinism

SCENARIO: Reproducible corpus
GIVEN a fixed integer seed
WHEN the generator and sampler produce a module and argument vectors
THEN a second run with the same seed produces the identical module and
    argument vectors (and therefore identical verdicts).

## DIFF-008 Bounded per-case and per-module runs

SCENARIO: Stalled case fails fast
GIVEN a single case whose oracle does not answer within its deadline
WHEN the sweep processes it
THEN the case is reported as an error (not a hang) and the wedged runtime
    process is torn down and a fresh one started.
SCENARIO: Stalled module does not wedge the sweep
GIVEN a module whose sweep exceeds the per-module deadline
WHEN the sweep runs
THEN the module is recorded as an error and the sweep continues.

## DIFF-009 Aggregate accounting

SCENARIO: Every case accounted for
GIVEN a completed sweep
WHEN it terminates
THEN every generated case is accounted for as PASS, FAIL, or a reported
    error, and the final line reports totals and any mismatch list.

## DIFF-010 Subset discipline

SCENARIO: Supported subset only
GIVEN the milestone-1 generators (random numeric expressions + fixed
    template modules)
WHEN a module is generated
THEN it has no imports and no `start`, uses only the milestone-1 scalar
    numeric and template feature set, and decodes, validates, and
    instantiates under wasm89, wasmtime, and wasm-interp with the same
    feature-flag set; a module that fails to validate in any engine is
    reported as a harness/config ERROR, never skipped or passed.

## DIFF-012 wasm-interp integer-only oracle

SCENARIO: Interpret used only where exact
GIVEN a case whose parameter and result types are all integer
WHEN compared against wasm-interp
THEN wasm-interp is an oracle for that case.
SCENARIO: Float cases skip wasm-interp
GIVEN a case with any float parameter or result
WHEN compared
THEN wasm-interp is not used as an oracle (its float output is not
    bit-exact); the float case is compared against wasmtime.

## DIFF-011 wasmtime decimal round-trip

SCENARIO: Decimal results decode to exact bits
GIVEN a finite f32/f64 result printed by wasmtime as a shortest-round-trip
    decimal
WHEN the harness decodes it back to bits
THEN the decoded bits round-trip to the identical printed decimal, and the
    value equals the engine's bits.
