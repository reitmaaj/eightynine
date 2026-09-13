# Stakeholders

- AS a compiler writer for a Hindley-Milner language
  I WANT to declare ordinary algebraic datatypes whose constructors become
  ordinary polymorphic HM bindings
  SO THAT I reuse libhm89 for inference without extending it.

- AS a compiler writer
  I WANT constructor patterns typed against a scrutinee type through ordinary
  unification
  SO THAT pattern-bound variables receive the correct inferred types.

- AS a compiler writer
  I WANT exhaustiveness and redundancy analysis over a closed constructor
  family
  SO THAT I can reject non-exhaustive matches and flag redundant clauses.

- AS a library maintainer
  I WANT `libadt89` to stay a strict ISO C89 sibling that is green-clean and
  depends only on the public `libhm89` API
  SO THAT neither library leaks internals to the other.

- AS a library maintainer
  I WANT a deterministic context/allocator/ownership model
  SO THAT lifecycle is predictable and `adt_ctx_destroy` never frees HM
  objects.
