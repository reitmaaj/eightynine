# 0004-algebraic-json-design

## Concrete carriers

- `N = double`, copied inline in `Number` nodes and through `map_number`.
- `S = j89a_str`, an immutable refcounted byte-string:
  `struct j89a_str { unsigned long refs; size_t len; char data[1]; }`.
  Both JSON string scalars and object member names use exactly this carrier.
- `Bool = enum { J89A_FALSE = 0, J89A_TRUE = 1 }`.

## Structural nodes

All structural nodes are immutable, individually refcounted heap objects.

```c
struct j89a_list   { unsigned long refs; int tag; /* NIL | CONS(json*, list*) */ };
struct j89a_object { unsigned long refs; int tag; /* EMPTY | MEMBER(str*,json*) | UNION(obj,obj) */ };
struct j89a_json   { unsigned long refs; j89a_kind kind; /* six payloads */ };
```

Ownership graph:

```
JSON/list/object nodes
    |-> refcounted structural children
    |-> refcounted byte-string carriers
double values copied inline
boolean values copied inline
```

Rules:

- Constructors/combinators (`cons`, `member`, `union`, `embed`) retain their
  operands; the returned node owns them. O(1).
- `map` builds immutable output incrementally and is NOT required to share
  original subtrees. On failure it releases the partial result.
- Object `union` is an O(1) `Union` node; object degeneracy is addressed only
  by representation flattening / explicit fold stacks that preserve the
  multiset exactly. Never dedupe, sort, collapse, or overwrite.
- No hash table or key index is built: the core never performs lookup.

## Error and allocator contract

```c
typedef enum j89a_status { J89A_OK = 0, J89A_NOMEM, J89A_CALLBACK, J89A_INTERNAL } j89a_status;

typedef struct j89a_alloc {
    void *(*alloc)(void *ctx, size_t n);
    void *(*realloc)(void *ctx, void *p, size_t n);
    void  (*free)(void *ctx, void *p);
    void *ctx;
    int failed; /* last NOMEM recorded here */
} j89a_alloc;
```

- Only operational failures exist. No `DUPLICATE`, `KEY_NOT_FOUND`,
  `BAD_NUMBER`, `UNICODE`, `ORDER`, `MERGE_CONFLICT` codes.
- Pure constructors return the value or `NULL` (`NULL` = `NOMEM`, recorded in
  the allocator). Callback-bearing operations return `j89a_status` and write
  results through out-parameters so `CALLBACK` propagates unchanged.

## R-runtime and fold-local containers

`json.fold`, `object.fold`, and `list.fold` type-erase an arbitrary result
carrier `R`.

```c
typedef struct j89a_rtype {
    size_t size;
    int  (*copy)(void *ctx, void *dst, const void *src); /* NULL => memcpy(size) */
    void (*drop)(void *ctx, void *value);                /* NULL => trivial */
    void *ctx;
} j89a_rtype;
```

Fold-local containers `j89a_rlist` (`L(R)`, ordered) and `j89a_robject`
(`B(S × R)`, bag, multiplicity preserved) are ephemeral, driver-owned inputs
to container callbacks. Their element accessors expose only `const void *`
plus the known `j89a_rtype`; the core never interprets `R` bytes.

Move semantics: each child `fold(child)` produces an owned temporary `R` which
the driver moves into the container node (no `copy` on the normal path). After
`array_case`/`object_case` returns, the driver drops every child `R` and
destroys the container; the callback returns its own owned `R` through its
`out_r` sink.

## Public API (namespace `j89a_`, header `include/j89_alg.h`)

List (element `j89a_json`): `j89a_list_nil`, `j89a_list_cons`,
`j89a_list_concat`, `j89a_list_fold`, `j89a_list_map`.

Object (`bag (S, json)`): `j89a_object_empty`, `j89a_object_member`,
`j89a_object_union`, `j89a_object_fold`, `j89a_object_map`.

JSON shape (six constructors): `j89a_json_project`, `j89a_json_embed`,
plus constructor aliases that are exact specializations of `embed`.

Recursive JSON: `j89a_json_fold`, `j89a_json_map`.

Ownership of every value-bearing parameter/result (borrow/retain/copy/move)
is documented in the header and in the binding-specific ownership notes.
Ownership mechanics never affect observable algebraic semantics.

## Const model (container constness)

`const` on an owning handle denotes CONTAINER constness, not transitive
constness. Representation payload/child fields are NON-const pointers to
independently owned, refcounted children. Reading a member of a `const`
container yields a non-const child pointer (C protects the field, not the
pointee), so a retain/release or sharing constructor can take that child
without a cast. Consequences:

- Read-only operations (`project`, `fold`, `map`, string readers) take
  `const` containers.
- Mutating operations (`retain`/`release`, sharing constructors) take
  non-const operands.
- A child read from a const parent is BORROWED; the caller must not release or
  invalidate it without first acquiring its own reference.
- No C-style cast ever discards `const` (green-cast-boundary clean).

## Fold-local R ownership states

Every local `R` slot in `fold.c`/`object.c` is one of:

```
UNINITIALIZED --successful callback/fold--> OWNED --moved into rlist/robject--> MOVED
```

Only OWNED values are passed to `j89a_rdrop`. After a successful move into an
R container the local is MOVED and is not dropped again. On any failure before
a successful move the OWNED temporary is dropped. `free_rlist`/`free_robject`
drop only OWNED nodes. This keeps catamorphism recursion leak- and
double-free-free under allocation and callback failure.

## Source shape (green-idiom)

`src/alg` follows the repository's green style: payload fields non-const,
effectful calls always bound to a variable (never returned directly), no
`&&`/`||`/`?:`/`,` operators, and small worker functions so nested control
bodies stay thin (at most one delegated call per run). `fold.c` is decomposed
last because it composes every ownership and callback convention. The result
passes `just green` (seven-cell matrix + clang-tidy semantic suite + format).

## Audits

- `just check-forbidden` / `scripts/alg-audit.sh forbidden`: `src/alg` must not
  reference scalar semantics (`strcmp/memcmp/strtod/isspace/tolower`, `<math.h>`,
  `<ctype.h>`, `<regex.h>`) or the `j89` parser/renderer/lookup symbols.
- `just api-audit` / `scripts/alg-audit.sh exports`: every exported symbol is
  `j89a_*` and none is named like a derived operation. The binding/lifetime
  support API (retain/release, string construction, allocator descriptor,
  `j89a_rtype`, fold-view access) is distinct from the ICD §38 semantic
  surface; only accidental derived semantic operations are prohibited.
- Exhaustive coverage uses a constructor-node size budget (small finite
  domain) rather than unbounded depth/branch enumeration.


## Hardening (correctness + contract pass)

- `j89a_str_new` bounds the allocation: payload `base = offsetof(data)`, fails
  cleanly (sets `al->failed`) when `len > (size_t)-1 - base - 1`.
- `NULL` never denotes an algebraic list/object/json value. An empty list is an
  allocated NIL node; `list_is_nil` tests the tag only; an OOM empty list is
  reported, never silently treated as a valid empty value.
- Callbacks with output parameters are transactional: on `J89A_OK` they
  initialize `*out` with one owned result; on non-OK `*out` is uninitialized
  and owns nothing; output storage must not alias borrowed inputs. Object
  member/combine callback order is unspecified and semantically meaningless.
- `j89a_rtype.copy` returns `j89a_status` (NULL => memcpy); `j89a_rcopy`
  propagates the status verbatim, so a copy `NOMEM` is not mislabelled
  `CALLBACK`.
- Programmer preconditions are documented (valid children, declared
  enumerators, valid function pointers, `rt->size > 0`); violations are out of
  contract, so the core raises only operational failures.
- New algebra-law/failure tests cover list concat identity/associativity, list
  map identity/composition, object union identity/commutativity/associativity
  and multiplicity, object fold invariance under rebracketing, failing
  `rtype.copy`, transactional garbage-after-failure, and overflow-sized
  strings.
- `j89.h` -> `j89_alg` import bridge (`src/bridge.c`, `include/j89_alg_bridge.h`)
  is a separate non-core layer (scalar/string/array->bag); the reverse
  (bag->ordered object) is intentionally absent because it needs an ordering
  policy.
