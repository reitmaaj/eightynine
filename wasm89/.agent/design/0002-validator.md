# Design: validator (spec ch3 + A.4)

## Entry points

`src/validate.h` / `src/validate.c`.

* `w89_module_validate(const w89_module *m) -> w89_err` — validates a
  decoded module; returns `W89_ERR_NONE` on success.
* `w89_validate_message(void) -> const char *` — the message of the last
  failure (static buffer, like `w89_last_illegal`).
* Pure helpers exposed for unit testing: `w89_typeenv_build`,
  `w89_type_canon_eq`, `w89_match_deftype`, `w89_match_valtype`,
  `w89_match_reftype`, `w89_match_resulttype`, `w89_match_comptype`.

## Type environment

The decoded `w89_rectype[]` is flattened into a `w89_typeenv`:

* each subtype becomes one `w89_deftype` with a consecutive global index,
  carrying a pointer to the decoded `w89_subtype` (comptype, finality,
  supertypes), its `recgroup` ordinal and `recpos` within the group;
* each `w89_recgroup` records `first` (first global index) and `nsubs`.

## Canonical equality

`w89_type_canon_eq(env, a, b)` mirrors `subst_deftype (subst_of c) dt1 =
subst_deftype (subst_of c) dt2`:

* if `a == b` -> equal;
* if `recpos` differs -> not equal;
* otherwise compare the two rec groups structurally
  (`recgroup_canon_eq`): subtype counts, finality, supertype lists and
  comptypes, member by member.

Type references inside a comptype/supertype are compared with group
awareness: a reference into the *containing* type's own group is a
position leaf (compared by `recpos` only); a cross-group reference is
compared by full canonical equality of the referenced types. Because
supertypes must precede their subtype, the rec-group reference graph is a
DAG and the comparison terminates. `final` participates, so two types that
differ only in finality are not equal.

## Subtyping

`w89_match_*` port `match.ml`:

* abstract heaptype lattice: `eq/struct/array/i31 <: any`, `struct/array/
  i31 <: eq`, `none <: t` iff `t <: any`, `nofunc <: t` iff `t <: func`,
  `noexn <: t` iff `t <: exn`, `noextern <: t` iff `t <: extern`;
* a concrete func/struct/array type index matches `func`/`struct`/`array`
  (and `any`/`eq` for struct/array);
* `w89_match_deftype(a, b)` = `a==b` or canonical-equal or some supertype
  of `a` is `match_deftype` to `b`;
* `match_reftype`: nullability `a<=b` (non-null does not match null) plus
  heaptype matching;
* `match_valtype`/`match_resulttype`: numtypes exactly; refs per above;
  same arity.

## Module validation

Context built in the reference's exact order (error order matters):
types, imports, tags, funcs (types), memories, tables, globals, datas,
elems; then function bodies, start, exports; duplicate-export check last.

* Type section: each rectype adds its deftypes to the context; per-subtype
  checks for forward use, final supertypes, comptype matching.
* Limits: `check_limits` with per-address-size maxima
  (i32: 2^16 pages / 2^32-1; i64: 2^48 / 2^64-1).
* Tag: result type must be empty ("non-empty tag result type").
* Const expressions: `is_const` (const, i32/i64 add/sub/mul, ref.null,
  ref.func, immutable global.get) then block-checked against the expected
  type ("constant expression required").
* `ref.func`: the declared-function set is computed from global/table
  inits, elem segments (including flags-0..3 index lists), and func
  exports; `ref.func x` requires `x` in that set ("undeclared function
  reference N").

## Instruction typing

Port of `valid.ml`'s `infer_resulttype`/`push`/`pop`/`peek` with `BotT`
and the ellipses (stack-polymorphic) flag. Context holds growable vectors
for locals (with `Set`/`Unset` initialization tracking), labels, and the
operand stack. Every decoded core op is typed; memidx and the memory64
address type are honored; alignment (`1 <= 2^align <= natural`) and i32
offset range are enforced. Error strings are formatted exactly like the
reference (`[i32 f64]`, `(ref null func)`, etc.).

## Lifetime

The typeenv is heap-allocated per validation and freed on return. The
decoded module must outlive the validation call (it does: the caller owns
it).
