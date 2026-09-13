# Acceptance criteria: explicit worklist (D3)

Additions to `.agent/acceptance/0000-acceptance.md`. Traces to
`.agent/testing/0006-worklist.md`.

## Must exhibit (exhibit)

- `fx_solve()` MUST saturate using an explicit queue of PENDING constraints;
  it MUST reach the same fixed point as the reference full-rescan engine.
- Only PENDING constraints are ever processed; a constraint satisfied in an
  earlier solve is not reprocessed.
- Repeated and incremental `fx_solve()` calls MUST be deterministic and MUST
  propagate newly added facts/bindings without duplicating consequences.
- Residual subset/join relations MAY remain PENDING after `fx_solve()` returns
  `FX_OK` (C8), unchanged by the worklist.

## Must reject / fail safely (reject)

- A semantic mutation during solving MUST re-enqueue the constraints it can
  affect (coarse: all PENDING in D3) so no reachable consequence is lost to
  scheduling order.
- The worklist MUST NOT change the semantic answer: no arbitrary branch on an
  underdetermined join, no dropped forced consequence, no `PENDING` treated as
  failure.
- The worklist machinery MUST NOT expose any new public symbol or engine type.
