# Design: calls, instantiation, imports (Phase B)

Extends the Phase A evaluator with function calls, frames, tail calls,
instantiation, imports, the spectest host, and the minimal
memory/table/segment/ref subset the Phase B conformance set requires
(call, fac, func_ptrs, start, imports*). Phase C completes the full
memory/table/bulk/ref surface.

## Store and instances

`w89_store` owns the store-side instances: funcinsts, globalinsts,
meminsts, tableinsts, taginsts, datainsts, eleminsts. Instances are heap
objects; module instances hold pointers into the store (so imported
instances share identity). The store also carries the type universe used
by cross-module matching.

```
w89_funcinst  = { is_host, typeidx, moduleinst *inst,
                  const w89_func *func,        (* ast *)
                  const w89_ft *ft, w89_host  (* host *) }
w89_meminst   = { limits, w89_byte *bytes, u64 npages }   (64KiB pages)
w89_tableinst = { tabletype, w89_ref *elems, u64 size }
w89_datainst  = { const w89_byte *bytes, u32 len }
w89_eleminst  = { w89_ref *refs, u32 n }
w89_externinst = { kind, union { funcinst*, globalinst*, meminst*,
                                 tableinst*, taginst* } }
w89_moduleinst.exports = { w89_name name, w89_externinst *ext }[]
```

`w89_moduleinst.datas/elems` retarget from the decoded segments to the
evaluated datainst/eleminst.

## Cross-module type matching

`w89_match_deftype_x(ae, a, be, b)` generalizes the validator's
within-env matching for types that live in different module instances:

* if `ae == be`, delegate to `w89_match_deftype`;
* otherwise compare canonical forms: rec-group size, per-position
  finality, supertype lists and comptypes, where an in-group type
  reference is a position leaf and a cross-group reference recurses
  cross-env. Termination follows the supertype DAG (as in the
  within-env `w89_type_canon_eq`).

`w89_match_valtype_x`, `w89_match_reftype_x`, `w89_match_limits` and
`w89_match_externtype` compose it for import matching and the
`call_indirect` type check.

## Invoke / Frame (port of eval.ml)

```
Call x         -> [Invoke (func inst x)]
CallRef x      -> null: trap "null function reference"
                  else [Invoke f]
CallIndirect x -> i = table load; oob: "undefined element N";
                  null: "uninitialized element N";
                  type check via match_deftype_x: else
                  "indirect call type mismatch, expected .. but got .."
ReturnCall/Indirect/Ref -> step the corresponding call, keep the
                  produced Invoke, emit ReturningInvoke(vs, f)

Invoke f, vs when budget = 0 -> Exhaustion "call stack exhausted"
Invoke f -> split n1 args; frame locals = params + default values;
            [Frame (n2, frame, ([], [Label (n2, [], ([], plain body))]))]
Frame (n, f', (vs', []))                       -> vs' @ vs, []
Frame (n, f', (vs', {Returning vs0}::_))        -> take n vs0 @ vs, []
Frame (n, f', (vs', {ReturningInvoke vs0, f}::_))-> take n1 vs0 @ vs, [Invoke f]
Frame (n, f', (vs', e'::_)) when is_jumping e'  -> vs, [e']
Frame (n, f', code')                            -> step with budget-1, re-wrap
```

## Minimal memory/table/segment/ref instructions

Phase B executes: `i32.load` (0x28), `i32.load8_u` (0x2D), `i32.store`
(0x36), `i32.store8` (0x3A), `memory.size` (0x3F), `memory.grow` (0x40),
`table.get` (0x25, for call_indirect), `ref.null` (0xD0), `ref.func`
(0xD2), `memory.init` (0xFC 8), `data.drop` (0xFC 9), `table.init`
(0xFC 0xC), `elem.drop` (0xFC 0xD). Traps: "out of bounds memory
access", "undefined element N", "uninitialized element N". Other
load/store/table/bulk/ref instructions remain decode+validate-only until
Phase C.

## Instantiation (`src/instantiate.c`)

`w89_instantiate(m, imports) -> moduleinst` follows the reference order:
types, imports (resolve + match), tags, functions, globals, tables,
memories, data, elems, exports; then fulfill funcinst module pointers
(for recursion); then evaluate active elem/data injection and the start
function in one config. `eval_const` reuses the evaluator with a
frame-less config over the const-expr instruction list.

`w89_register(name, inst)` and `w89_lookup_export(inst, name, kind)`
support the REPL and the driver. `w89_invoke(f, args)` checks argument
count/types ("wrong number of arguments"/"wrong types of arguments"),
then runs a config with `[Invoke f]`, mapping exhaustion to "call stack
exhausted".

## Host module (`src/host.c`)

The spectest instance is built once into the store: host funcs
`print*` (bare stdout lines), `abort`, globals 666/666.6, `memory` 1x2
pages, `table` 10x20 funcref.

## REPL and driver

`wasm89 repl` reads commands from stdin:

```
module <path>              -> @ok | @error <msg>
register <name>            -> @ok | @error <msg>
invoke <name> <func> <n> <arg>...           -> @return <val>... | @trap <msg> | @exhaustion
get <name> <global>                          -> @return <val> | @error <msg>
assert_return <name> <func> <nargs> <arg>... <nexp> <exp>...  -> @pass | @fail <details>
assert_return_get <name> <global> <exp>      -> @pass | @fail <details>
assert_trap <name> <func> <nargs> <arg>... <msg>               -> @pass | @fail <details>
assert_exhaustion <name> <func> <nargs> <arg>...               -> @pass | @fail <details>
quit
```

Value tokens are `<type>:<hex-bits>` for i32/i64/f32/f64 and
`ref.null:<ht>` / `ref.func:<idx>` / `ref.extern:<hex>` for references.
Host print output is written to stdout as bare lines; the driver
recognizes only `@`-prefixed result lines. `spec_driver.py` is rewritten
to run one persistent `wasm89 repl` and translate each JSON command
(Python converts wast float text to IEEE bits). Phase D extends the same
protocol with NaN/either/ref result patterns and `assert_exception`.
