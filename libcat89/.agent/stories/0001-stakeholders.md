# libcat89 stakeholders

## 0001-stakeholders.md

- AS a **C library author building on the generic kernel**, I WANT a minimal,
  representation-neutral category core with a stable ABI, SO THAT my own
  categories (symbolic, graph-, table-, or function-backed) plug in without
  adapting to derived-structure assumptions.

- AS a **consumer implementing a concrete category backend**, I WANT a clear
  callback contract (dom/cod/identity/compose, structural object identity,
  morphism retain/release) SO THAT I can promise the categorical laws and let
  the library guarantee only mechanical typing.

- AS a **consumer modelling derived structure**, I WANT isomorphisms, split
  morphisms, functors, natural transformations, limits, and generic
  constructions as ordinary wrappers over `cat89_mor`/`cat89_obj` SO THAT the
  same morphism can participate in many structures and the category core never
  changes to admit them.

- AS a **verifier of categorical law**, I WANT optional checkers that explicitly
  take the equality/enumeration capabilities they need SO THAT laws can be
  checked when data supports it but are never silently assumed by the core.

- AS a **user of reference categories**, I WANT finite (table), free (path),
  and thin (preorder) backends with full enumeration/equality SO THAT I can
  build small exhaustive mathematical oracles for testing every layer.

- AS an **application mapping representations to translations** (e.g. Rosetta),
  I WANT to represent representations as objects and translations as morphisms,
  and exact/reversible as `cat89_iso`, SO THAT categorical structure is shared
  while application semantics (loss, cost, execution, proofs) stay external and
  never force a core change.

- AS a **maintainer**, I WANT every behavior change to follow
  scenario -> failing test -> minimum code -> refactor, with the finite oracle
  and exhaustive checker in place before higher layers SO THAT each layer is
  proven compositionally and `main` stays green.
