# 0004-algebraic-json-core

The `j89_alg` module is a policy-neutral algebraic core for JSON processing,
added as a sibling additive API to the practical `j89` document model.

## Purpose

Answer only:

> Which algebraic structures follow from the JSON value constructors
> themselves, and which canonical constructors, morphisms, and eliminators do
> those structures induce?

The core implements the fixed point

```
J = μX.( 1 + 2 + N + S + L(X) + B(S × X) )
```

with

- `N` the number carrier,
- `S` the byte-string carrier,
- `L` the finite-list (free monoid / list functor),
- `B` the finite-bag (free commutative monoid / bag functor).

## Additivity rule

`j89_alg` and the existing `j89` model solve different problems and do not
share a representation.

- `j89`: practical JSON document model. Unique object keys, lookup, parsing,
  rendering, duplicate-key rejection.
- `j89_alg`: policy-neutral JSON constructor algebra. Objects are bags of
  member occurrences; scalar semantics stay opaque.

The rules are:

- `j89_alg` MUST NOT depend on the `j89` parser, renderer, object lookup, or
  duplicate rejection.
- Interoperability occurs only through explicit policy-bearing adapters
  outside the algebraic core (`j89 -> j89_alg` is injective; `j89_alg -> j89`
  requires a duplicate-name policy: reject / first / last / custom).

## Concrete realization

C89 has no parametric polymorphism. The concrete realization is documented in
the public header:

```
J = μX.( 1 + 2 + double + bytestring + L(X) + B(bytestring × X) )
```

- `N = double` (copied inline)
- `S = j89a_str` (immutable refcounted byte-string)
- `Bool = enum { J89A_FALSE, J89A_TRUE }`

Semantic opacity, not representation opacity: the core stores, copies,
retains, releases, and transports `N` and `S` but never compares, orders,
classifies, formats, decodes, or hashes them.

## What the core is not

The core is not a conventional collection standard library. No lookup,
indexing, slicing, filtering, searching, updating, merging, arithmetic,
Unicode manipulation, paths, parsing, or rendering belong here.
