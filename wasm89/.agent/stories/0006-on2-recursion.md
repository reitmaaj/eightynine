# Stories

*Stakeholder value for the O(N) iterative-control-machine work. Drives
`.agent/testing/0014-on2-recursion.md`, `.agent/acceptance/0014-on2-recursion.md`
and design `.agent/design/0007-iterative-machine.md`.*

## 0006 - User gets O(N) deep non-tail recursion on a bounded native stack

As a user running deeply recursive, non-tail WebAssembly functions,
I want recursion depth to be bounded by the configured call budget rather
than by native C-stack size, and for each instruction to execute in
constant amortized time,
so that terminating recursion to depth 5000 completes quickly and
predictably (O(N), not O(N^2)) without risking a native stack overflow.

## 0007 - Maintainer keeps a semantic no-op behind the rewrite

As a maintainer rewriting the evaluator's control-flow orchestration,
I want the rewrite to be migrated stage-by-stage, side-by-side with the
legacy recursive path, keeping evaluation results, trap behaviour,
exception propagation and multi-value semantics unchanged,
so that every stage stays green and the full conformance sweep is
bit-for-bit the same as before the rewrite.
