# libfx89 concept

`libfx89` is a small, standalone ISO C89 library for the *static and abstract
semantics of effects*. It represents and reasons about effect requirements;
it does not execute them.

The three sibling semantic toolkits stay peers:

```text
libhm89     value-type inference
libadt89    nominal algebraic datatypes
libfx89     effect rows, effect polymorphism, effect constraints
```

No library depends on another. A host language front end composes whichever
it needs.

`libfx89` owns only the algebra of effect requirements:

- nominal effect kinds and their operation metadata;
- effect atoms (nominal, and host-parameterized later);
- finite effect rows `{IO, State}` and open rows `{IO | e}`;
- effect-row variables and effect-only polymorphism;
- equality, subset/inclusion, membership, absence/lacks, and exact set-union
  constraints;
- substitution, normalization, generalization, instantiation;
- a monotone worklist solver with checkpoints and diagnostics.

The central object mirrors `hm_type`: the **effect row**

```text
{Console, Random}
{IO | e}
```

Rows obey canonical ordering and a **disjoint-tail invariant**: an explicit
head never overlaps its unresolved tail, giving every open row a unique
canonical decomposition.

`libfx89` deliberately knows nothing about value types, terms, ASTs, JSON,
evaluation order, continuations, handlers, or capabilities. Each of JML,
Hoodlum, and JJ supplies that interpretation:

- JML infers an effect row per expression and checks `required <= granted`;
- Hoodlum tags foreign-source/scan clauses with a coarse effect;
- JJ assigns effect rows to actions and composes them by exact union;

while `libfx89` supplies only the shared effect-domain machinery.

Dependency: none. `libfx89` is standalone.
