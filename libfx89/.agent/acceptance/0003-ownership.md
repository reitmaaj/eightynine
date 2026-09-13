# Acceptance criteria: ownership and contract hardening

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0003-ownership.md`.

## Must exhibit (exhibit)

- `fx_ctx_set_atom_domain` MUST copy the domain vtable by value into the
  context and MUST reject a domain missing its `compare`, `copy`, or
  `destroy` callback with `FX_ERR_INVALID`.
- Opaque objects MUST carry their owning context so a public operation can
  reject a mixed-context operand deterministically.

## Must reject / fail safely (reject)

- `fx_require_*` MUST return `FX_ERR_INVALID` when handed a row or atom owned
  by a different context, rather than invoking one context's atom-domain
  callback on another context's parameter representation.
- `fx_atom_equal` MUST NOT compare atoms from different contexts; it MUST
  report unequal.
- An allocation whose `count * sizeof` product overflows the address-space
  size MUST fail safely as out-of-memory, never wrap to a small allocation.
