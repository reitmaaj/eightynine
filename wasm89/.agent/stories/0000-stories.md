# Stories

*Files follow the pattern `NNNN-...md`; multiple stories are bundled here.*

## 0001 - Embedder runs spec-exact WebAssembly

As an application developer embedding a WebAssembly runtime,
I want to decode, instantiate and invoke Wasm modules with bit-exact
specified behaviour,
so that execution results are trustworthy and portable across hosts.

## 0002 - Maintainer keeps strict C89 and warning-free builds

As a systems programmer maintaining this runtime,
I want the whole codebase to compile under
`-std=c89 -pedantic-errors -Wall -Wextra -Werror` with zero warnings,
so that the runtime is portable to C89 toolchains and free of sloppiness.

## 0003 - Conformance engineer measures compliance objectively

As a conformance engineer,
I want the official `WebAssembly/testsuite` to be runnable against this
runtime in an automated, deterministic way,
so that compliance claims are backed by evidence, not assertion.

## 0004 - Developer gets fast, deterministic feedback

As a developer,
I want every change to be validated by `just lint`, `just build` and
`just test` locally in seconds,
so that defects are caught early and cheaply.

## 0005 - User of floats gets reproducible results

As a user relying on floating-point behaviour,
I want all NaN results to be canonical positive NaNs and relaxed SIMD
instructions to behave deterministically,
so that repeated runs produce identical results.
