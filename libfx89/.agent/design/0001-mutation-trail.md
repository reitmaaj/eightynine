# Design: mutation-trail invariants (D2 follow-up)

Status: recorded follow-up, not yet implemented. This documents the intended
contract for replacing the checkpoint snapshots with a generic undo trail
while preserving the current solver semantics exactly.

## Goal

Rollback must restore the exact semantic solver state captured at a
checkpoint. Today that is done by snapshotting variable facts/bindings and the
completion state of every constraint (`.agent/design/0000-design.md`).
A mutation trail achieves the same observable effect in `O(changes since
checkpoint)` and supports nested checkpoints naturally.

## Trail records

Each record names one semantic mutation and enough to undo it, applied in
reverse on rollback. Candidate record kinds:

- `TRAIL_BIND` — a representative's `binding` changed; undo restores the prior
  binding pointer (or NULL).
- `TRAIL_REQ` / `TRAIL_FORB` — a required/forbidden atom was added; undo
  decrements that representative's length (equivalently, snapshots of
  `nreq`/`nforb` become unnecessary).
- `TRAIL_STATE` — a constraint's completion state changed; undo restores the
  prior `fx_constraint_state`.
- `TRAIL_CONFLICT` — the `last_conflict` fields changed; undo restores prior
  conflict kind and provenance.
- `TRAIL_PARENT` / `TRAIL_RANK` (D3) — union-find parent/rank changed; undo
  restores them (enables path-compression-free trailed union-find).
- List insertions do not need records when a checkpoint stores the list marks:
  `variable_mark`, `constraint_mark`, `constraint_head`.

## Markers

A checkpoint is a triple:

```text
trail_mark        index into the global trail log
variable_mark     ctx->vars at checkpoint time
constraint_mark   ctx->constraints_tail at checkpoint time
constraint_head   ctx->constraints at checkpoint time (for the empty case)
```

`fx_ctx_checkpoint` records the current marks and returns a token. Rollback
pops trail records from the tail down to `trail_mark`, then restores
`ctx->vars` to `variable_mark`, and cuts/clears the constraint list to
`constraint_mark`/`constraint_head`.

## Invariants

- **Trail order equals mutation order.** Every semantic mutation that must be
  reversible appends one record before (or atomically with) mutating state. A
  failed append (NOMEM) must abort the mutation, never leave a partial
  mutation on the trail.
- **LIFO.** Only the top checkpoint may roll back or commit. Commit forgets
  the marker but leaves records above it in place, so a later rollback to an
  outer checkpoint undoes inner changes too.
- **Monotonicity between checkpoints.** The solver is monotone in a live
  (non-rolled-back) interval; rollback is the only reversal, via the trail.
- **Rolled-back objects are unreachable, not freed.** Vars/constraints created
  after a checkpoint are removed from the lists; their trail records, popped
  in reverse, touch objects that are no longer reachable but are harmless (the
  arena frees all blocks at `fx_ctx_free`).
- **Records are idempotent to the reachable state.** Popping in reverse
  restores lengths/pointers so that re-solving reaches the same fixed point as
  solving after an equivalent fresh construction.

## Non-goals / to revisit

- Path compression is intentionally deferred to a later, profile-justified
  change (see `0000-design.md` and the D3 note).
- Watch/worklist registration is out of scope for D2; registrations become
  relevant only in D4/D5 once representatives are stable.

## Verification

Reuse and extend `.agent/testing/0001-join-rollback-purity.md` rollback
scenarios: the trail must reproduce the same observable state as today's
snapshots for rollback of facts, bindings, constraint completion, conflicts,
and nested checkpoints. A differential oracle (see
`.agent/design/0002-solver-oracle.md`) comparing snapshot-based and trail-based
rollback on cloned problems would strengthen this.
