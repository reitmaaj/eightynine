# Testing: GC — differential (S3.2.C4)

*BDD scenarios for exercising GC references in the differential harness
(PLAN S3.2). Design: `.agent/design/0008-differential.md`,
`.agent/design/0011-gc.md`. Differential is enabled only after the
interpreter runs GC, so a mismatch is a real finding.*

## GC-006 GC ref identity/equality semantics match the engines

SCENARIO: Deterministic GC outcomes compare across engines
GIVEN a GC program whose observable results are booleans/values derived
    from ref identity, `ref.eq`, and casts (not object addresses)
WHEN run under wasm89 and under the engines (wasmtime + wasm-interp) with
    the GC feature flag enabled
THEN observable results compare equal; object identity is compared only by
    `ref.eq`/`ref.test` semantics, never by raw address.

SCENARIO: No GC emit before engine parity
GIVEN the module generator before the engine GC flag/rule is wired
WHEN it runs
THEN it MUST NOT emit GC/struct/array/i31 programs
AND once wired, templates cover the implemented GC ops using
    address-independent observables.

SCENARIO: GC traps are interchangeable
GIVEN a GC runtime/trap case (null access, cast failure, array OOB)
WHEN run under wasm89 and the engines
THEN a runtime failure on both is acceptable (rt≈rt); a pass-vs-fail or an
    observable-result mismatch is a finding.
