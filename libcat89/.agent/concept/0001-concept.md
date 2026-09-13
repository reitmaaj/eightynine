# libcat89 concept

## 0001-libcat89-concept.md

`libcat89` is a small, representation-neutral generic category-theory
substrate for C89. It models a category directly as

    (Ob, Mor, dom, cod, id, compose)

and layers derived categorical constructions (functors, natural
transformations, isomorphisms, split morphisms, diagrams/cones/limits, and
generic category constructions) strictly above that kernel.

### The category kernel

The core is exactly the mathematical definition of a category: obtain
domain/codomain, construct an identity, compose compatible morphisms. It
requires no more computation. The core is representation neutral: objects and
morphisms are opaque handles (`cat89_category`, `cat89_obj`, `cat89_mor`) with
no required layout.

Every backend additionally answers two raw-handle provenance probes,
`owns_obj`/`owns_mor`: is this live handle presently valid for this category?
Generic wrappers check provenance before any representation-specific callback,
so a foreign handle can never be cast, retained, compared or composed by the
wrong category.

### What is deliberately excluded from the core

The category core must not assume or require:

- finiteness or enumeration of objects/morphisms/hom-sets;
- decidable equality of objects or morphisms;
- that morphisms are C functions or execute on values;
- that morphisms carry IDs, hashes, or canonical forms;
- any derived structure (iso, mono/epi, product/limit, functor, nat,
  adjunction, monad, enrichment).

Such facilities exist only as optional capabilities or as modules layered
above the core. Laws (associativity, identity, functor laws, naturality,
inverse laws, universal factorization) are **contracts** promised by backends,
not runtime requirements on the core. Optional checkers may verify them when
the required capabilities are available.

### Three-part architecture

    minimal category core
    + strictly layered categorical structures
    + orthogonal computational capabilities

Concrete reference backends (finite, free, thin) act as mathematical oracles
and fixtures. The finite backend plus an exhaustive law checker are built early
so that every higher layer is verifiable compositionally against small
exhaustive models.

### Intended role

`libcat89` is a categorical algebra substrate for consumers such as a future
translation/reference system ("Rosetta"), which must extend `libcat89`
externally (representations become `cat89_obj`, translations become `cat89_mor`,
reversible pairs become `cat89_iso`, one-sided round trips become
`cat89_split_*`) without any change to `cat89_category`/`cat89_mor`. No
application semantics (information loss, cost, execution, proofs, provenance)
enter the generic ABI.
