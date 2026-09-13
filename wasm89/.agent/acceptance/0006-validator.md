# Acceptance: validator

*Specification reference: WebAssembly Core Spec 3.0, chapter 3 and appendix
A.4.*

## MUST

* `w89_module_validate` takes a decoded `w89_module` and returns
  `W89_ERR_NONE` iff the module is valid per spec ch3.
* Validation follows the reference interpreter's order: types, imports,
  tags, functions (types only), memories, tables, globals, data segments,
  element segments, then function bodies, start function, exports, and
  finally duplicate-export-name checking. The first defect in that order
  is the one reported.
* The flattened type list is derived from the decoded rectypes; each
  subtype receives a consecutive global index; each rec group records its
  first index and subtype count.
* Canonical type equality follows `subst_deftype (subst_of)`: in-group
  type references are compared by position within the rec group,
  cross-group references by full canonical equality of the referenced rec
  groups. `final`, supertype lists, and comptypes participate in equality.
* Subtyping follows the abstract heaptype lattice and the
  canonical-equality-or-supertype rule for deftypes; `null` vs non-null
  matches only when the target is nullable.
* A subtype's supertype index MUST be strictly less than its own index
  ("forward use of type N in sub type definition"), MUST NOT be final
  ("sub type N has final super type M"), and its comptype MUST match
  ("sub type N does not match super type M").
* Memory limits: min/max MUST be at most 2^16 pages (i32) or 2^48 pages
  (i64) ("memory size must be at most ..."). Table limits: at most 2^32-1
  (i32) or 2^64-1 (i64). In both cases min MUST NOT exceed max ("size
  minimum must not be greater than maximum").
* Constant expressions MUST contain only const, i32/i64 add/sub/mul,
  ref.null, ref.func, or global.get of an immutable global ("constant
  expression required"), and MUST type-check against the expected type.
* A `ref.func` index MUST be declared, i.e. referenced by a global/table
  init, an elem segment, or an export ("undeclared function reference N").
* Function bodies MUST type-check against the reference's stack-typing
  rules, including labels, locals (with defaultability for
  non-defaultable ref locals: "uninitialized local"), results, and the
  polymorphic stack for unreachable code.
* Alignment: `1 <= 2^align <= natural_size` ("alignment must not be larger
  than natural"); i32-memory offsets MUST be below 2^32 ("offset out of
  range").
* Error strings MUST match the reference interpreter's messages (as
  substrings accepted by the official testsuite), e.g. "unknown function
  N", "type mismatch: instruction requires [...] but stack has [...]",
  "immutable global", "non-empty tag result type", "start function must
  not have parameters or results".
* The CLI `load` command decodes then validates; a valid module prints
  "ok", an invalid one prints "error: <message>".
* `w89_validate_message` returns the message of the last validation
  failure.

## MUST NOT

* The validator MUST NOT accept out-of-scope instructions (SIMD/GC): those
  still fail at decode as "unsupported feature" / "illegal opcode".
* The validator MUST NOT accept a module whose functions reference unknown
  types, or whose instructions reference unknown labels/locals/functions/
  globals/tables/memories.
* The validator MUST NOT accept limits above the per-address-size maxima,
  min > max, or invalid alignments/offsets.
* The validator MUST NOT report success when any function body, const
  expression, or module field fails validation; the first error in the
  reference's order MUST be reported.
