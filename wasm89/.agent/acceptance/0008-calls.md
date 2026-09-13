# Acceptance: calls, instantiation, imports (Phase B)

*Acceptance criteria for the interpreter milestone Phase B. Reference:
WebAssembly Core Spec 3.0 chapter 4, the reference interpreter's
`eval.ml` (Call/CallIndirect/CallRef/ReturnCall*/Invoke/Frame cases) and
the instantiation functions (init\_type through init\_export, run\_elem,
run\_data, run\_start).*

## MUST

* `w89_funcinst` MUST distinguish ast functions (decoded code owned by a
  module) from host functions; ast functions MUST carry their type index,
  their module instance (for type lookups and recursion), their locals,
  and their body.
* `Invoke` MUST split the parameter values off the operand stack, build a
  frame whose locals are the parameters followed by default values for
  the remaining locals, and push a `Frame` admin instruction wrapping a
  fresh `Label` over the function body. `Frame` MUST unwind by merging
  results on normal completion, by `take n vs0` on `Returning`, by
  re-invoking on `ReturningInvoke`, and MUST propagate jumping
  instructions outward while discarding the pending body. The frame step
  MUST decrement the budget; an `Invoke` at budget zero MUST fail with
  exhaustion "call stack exhausted".
* `call`/`call_indirect`/`call_ref` MUST produce an `Invoke` of the
  resolved function instance. `call_indirect` MUST trap with "undefined
  element N" for out-of-bounds table indices, "uninitialized element N"
  for in-bounds null slots, and "indirect call type mismatch, expected
  ... but got ..." when the callee's type does not match the expected
  type (using cross-module type matching). `call_ref` MUST trap with
  "null function reference" on a null reference.
* `return_call`/`return_call_indirect`/`return_call_ref` MUST produce a
  `ReturningInvoke` carrying the results and callee, so the current frame
  is replaced rather than nested.
* `ref.null` and `ref.func` MUST produce null / function references;
  `ref.func` MUST resolve against the current module instance.
* Memory instances MUST hold their bytes and page count; `memory.size`/
  `memory.grow` MUST behave per spec (grow returns the old size or -1,
  zero-fills new pages, respects the declared max). Loads/stores MUST
  trap with "out of bounds memory access" when the access is out of
  bounds, and MUST honor the effective address (offset + index).
* Active data/element segments MUST be evaluated at instantiation
  (offset const expression, then copy / drop), with bounds enforced.
* `w89_instantiate` MUST follow the reference's order: types, imports,
  tags, functions, globals, tables, memories, data, elems, exports, then
  func-instance fulfillment, then eval of elem/data injection and the
  start function. A start function with parameters/results is rejected
  at validation (already enforced).
* Import resolution MUST look up module name then export name in the
  registry (including the pre-registered "spectest"), failing with
  "unknown import ..." when absent. Import type matching MUST use
  cross-module type matching (structurally equal functions types,
  equal mutability and value type for globals, limits subtyping for
  memories/tables), failing with "incompatible import type for ..." on
  mismatch.
* `w89_register` MUST make a module's exports importable under a name;
  registered instances MUST share instance identity with importers.
* The spectest module MUST provide `print`/`print_i32`/`print_i64`/
  `print_f32`/`print_f64`/`print_i32_f32`/`print_f64_f64`/`abort` host
  functions, globals `global_i32`/`global_i64` = 666,
  `global_f32`/`global_f64` = 666.6, `memory` 1 page (max 2), and
  `table` 10 funcref (max 20). Host `print*` MUST write bare lines to
  stdout.
* The `wasm89 repl` command MUST speak the `@`-prefixed line protocol
  (`@ok`, `@error <msg>`, `@return <values>`, `@trap <msg>`,
  `@exhaustion`, `@pass`, `@fail <details>`) and MUST tolerate bare
  stdout lines (host prints) interleaved with result lines.
* The driver MUST run the Phase B conformance files (call, fac,
  func_ptrs, start, imports*) with exact-bit result comparison.

## MUST NOT

* The evaluator MUST NOT return results from a callee frame while
  leaving the frame or its body pending; frames MUST unwind completely.
* The evaluator MUST NOT grow a memory past its declared max or the
  address-space limit without returning -1.
* The evaluator MUST NOT succeed a load/store past the memory bounds.
* Instantiation MUST NOT succeed when an import's type does not match,
  when an import is unknown, or when an active segment's range exceeds
  the memory; each MUST fail with the reference message.
* The REPL MUST NOT confuse host print output with command results:
  result lines MUST always be `@`-prefixed.
* The interpreter MUST NOT be reachable from the CLI except through
  `wasm89 load` (decode+validate) and `wasm89 repl` (decode+validate+
  instantiate+execute); the driver MUST use `repl` for execution.

## Full-suite sweep status (Phase B)

The full spec sweep is green for every file whose instructions are in
scope. Execution commands that hit not-yet-implemented features are
skipped by the driver (messages containing "unsupported"/"not
implemented", and ref-typed result patterns). The remaining failing
files are exclusively Phase C/D features: memory* (loads/stores beyond
the B subset, copy/fill/init, memory64), table* (size/grow/fill/copy/
init, table64), bulk*, elem, exports, instance, linking*, float_memory*,
load1, memory-multi, data_drop0, and simd_linking (v128). Each of these
is reconciled in Phase C (memory/table/bulk/ref) and Phase D
(exceptions + message reconciliation).
