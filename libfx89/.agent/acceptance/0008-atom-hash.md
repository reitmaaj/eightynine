# Acceptance criteria: hash atom interning (D6)

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0008-atom-hash.md`.

## Must exhibit (exhibit)

- Parameterized-atom interning MUST use the atom-domain `hash` to restrict the
  equivalence scan to one bucket, and MUST still decide equivalence by the
  `compare` callback (correctness never relies on hash equality alone).
- Equal parameters MUST intern to one atom; unequal parameters MUST remain
  distinct, including under hash collisions.
- A domain without a `hash` callback MUST still intern correctly (bucket keyed
  by kind).

## Must reject / fail safely (reject)

- Interning MUST NOT compare atoms from different contexts or invoke one
  domain's callback on another domain's parameter representation (ownership
  rules from 0003 still apply).
- The bucket index MUST NOT change semantics: the parameterized atom's
  kind/parameter and `fx_atom_equal` results are unchanged.
