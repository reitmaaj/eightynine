# Acceptance criteria: mutation-trail rollback (D2)

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0005-mutation-trail.md` and the design
`.agent/design/0001-mutation-trail.md`.

## Must exhibit (exhibit)

- Rollback MUST restore variable facts/bindings, constraint completion state,
  and last-conflict state via undo records applied in reverse, reproducing the
  exact state of the snapshot-based rollback it replaces.
- Rollback cost MUST be proportional to the mutations since the checkpoint,
  not to the total number of variables/constraints.
- Nested checkpoints and commit MUST behave identically to the previous
  snapshot semantics.

## Must reject / fail safely (reject)

- A failed trail append (NOMEM) MUST abort the corresponding mutation so the
  trail and the live state never diverge; it MUST NOT leave a partial mutation
  on the trail.
- Rollback MUST NOT leave the trail index or any list mark inconsistent with
  the restored solver state.
