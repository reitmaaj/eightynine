# libcat89 design

## 0001-design.md

### Header layering (dependency DAG, acyclic)

    status / alloc
        |
       core
        |
    +---+---------------------------+---------+--------+--------+
    |           |                   |         |        |        |
   eq  enum   functor            iso     split    free    thin opposite product
        |       |                 |
        |       v                 |
        |      nat                |
        |       |                 |
        +---+---+--------+--------+
            |            |
          check        comma
            |            |
            |        slice coslice arrow
          finite
    functor -> diagram -> cone -> limit

`core.h` includes none of the higher headers. Module deps only point downward.

### Core opaque types and category representation

    typedef struct cat89_category cat89_category;
    typedef struct cat89_obj      cat89_obj;
    typedef struct cat89_mor      cat89_mor;

Private: `struct cat89_category { unsigned long refs; cat89_category_ops ops;
void *ctx; cat89_allocator allocator; }`. `cat89_obj`/`cat89_mor` are
backend-defined opaque handles with no generic layout.

`cat89_category_ops` supplies dom/cod/identity/compose, `obj_same`
(structural object identity for typing, an equivalence relation, never public
equality), `mor_retain`/`mor_release` (morphisms carry reference semantics),
mandatory `owns_obj`/`owns_mor` (raw-handle provenance: is this live handle
presently valid for this category?), and optional `category_destroy`.

The generic wrappers check provenance before any representation callback:
`dom`/`cod`/`identity`/`compose`/`obj_same`/retain/release reject foreign
handles with `CAT89_INVALID` (0 for `obj_same`, no-op for release). In
particular `cat89_compose(c,g,f,&out)` checks ownership of both operands before
calling `cod(f)`/`dom(g)`, then validates `cod(f)` vs `dom(g)` via `obj_same`
and returns `CAT89_DOMAIN` on mismatch, then calls backend `compose`.
Composition convention throughout is `out = g o f`.

### Status and ownership conventions

One status domain (`cat89_status`); callbacks return `cat89_status`; layers
propagate verbatim. Pointer out-params are set to `NULL` before work; integer
out-params to `0`. `cat89_mor **out` => one owned reference; `const cat89_mor *`
/ `const cat89_obj *` => borrowed. No public function consumes caller input
ownership. Release functions no-op on `NULL`.

### Capabilities are separate objects

`cat89_eq` and `cat89_enum` are independent capability objects retaining their
category. Enumeration supports object/morphism/hom iterators; individual ops may
be `NULL` (absent -> `CAT89_NOT_SUPPORTED`). Morphism iterators return owned
references; object iterators return borrowed handles. No capability state lives
on `cat89_category`.

### Derived structures are external wrappers (CAT-I3)

`cat89_iso` holds category + forward/inverse ordinary morphisms (retained).
`cat89_split_mono/epi` analogous. No morphism flag/tag is added; no core change
is required to add any derived structure. Wrappers implement ordinary
`cat89_category_ops` and are substitutable wherever a `cat89_category *` is
accepted (ICD-I10).

### Diagrams are functors

`typedef cat89_functor cat89_diagram;` (semantic alias; no duplicate object
model). Cones/cocones retain their diagram; a limit owns a cone plus a
`factor(candidate, &u)` callback returning the unique mediating morphism.
Uniqueness is contractual; explicit finite checkers verify existence and
uniqueness exhaustively.

### Wrapper object/morphism storage

Objects have no release API, so a wrapper category owns all object wrappers in
an append-only arena valid for the category lifetime (ICD-I4). Morphisms are
independently refcounted wrappers destroyed at zero, avoiding permanent
category->morphism->category cycles (categories do not own morphism wrappers).

### Internal helpers

Checked size arithmetic (`cat89_size_add/mul`), reference helpers
(`cat89_ref_inc/dec`, no wraparound, `ULONG_MAX` guard), and a private growable
vector live under `src/` and never in the public ABI. No runtime kind tags, no
global mutable scratch state (reentrancy, ICD-I9).

### Concrete backends

- `finite`: table of objects/morphisms, dom/cod/identity/composition. The
  builder stores one morphism-row vector and one composition-row vector, so each
  mutation is a single checked append (failure-atomic). `cat89_finite_validate`
  checks the complete finite-category table; `cat89_finite_build` validates
  first, never consumes the builder, and rejects invalid tables with
  `CAT89_INVALID`. A test-only unchecked build backs intentionally partial
  fixtures.
- `free`: objects wrap graph vertices; morphisms are flat immutable edge
  vectors (paths); identity = empty path; distinct paths stay distinct.
- `thin`: objects wrap preorder values; at most one morphism a->b iff `leq(a,b)`.

### Reference fixtures

terminal, D2 discrete, walking arrow, walking composable pair/triple, C2
groupoid, parallel pair, diamond poset (and a non-lattice poset for NOT_FOUND).
