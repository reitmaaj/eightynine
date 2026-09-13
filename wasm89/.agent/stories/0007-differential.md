# Stories: differential harness

*New bundled story file for cross-engine differential testing of wasm89.*

## 0007 - Engineer catches semantic drift against production engines

As a runtime maintainer,
I want every supported numeric program to be executed by wasm89 and by
at least one independent WebAssembly engine (wasmtime, and the spec
reference interpreter wasm-interp) and the observable results compared,
so that a divergence from well-tested engines is surfaced as a concrete,
reproducible failure rather than passing silently.

## 0008 - Engineer gets reproducible, bounded differential runs

As a runtime maintainer,
I want the differential generator and its input corpus to be fully
deterministic from a seed, and every run to be bounded per case and per
module with clear progress and aggregate accounting,
so that a reported mismatch can be reproduced verbatim and a pathological
case cannot wedge the whole run.

## 0009 - Engineer trusts strict float equivalence

As a user depending on floating-point determinism,
I want finite float results to agree bit-for-bit with reference engines
and any NaN result to be treated as equal to another NaN regardless of
payload,
so that the deterministic profile is verified without flagging legitimate
cross-engine NaN-payload differences.

## 0010 - Maintainer treats missing engines as an error, not a pass

As a CI operator,
I want the harness to refuse to run (reporting a clear error and a
nonzero exit) when a configured reference engine is not installed,
so that an environment without the engines can never report a false all-green.
