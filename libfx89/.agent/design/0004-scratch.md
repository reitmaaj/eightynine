# Design: scratch arena — deferred (D5)

Status: recorded follow-up, deferred. Implementing a resettable scratch arena
was considered for the "arena grows with query/solver activity" observation,
and declined because it conflicts with the ownership contract.

## Why deferred

`fx89.h` and `.agent/design/0000-design.md` state that every object is owned by
the context and freed by `fx_ctx_free`; query results (e.g. the normalized row
returned by `fx_var_binding`, `fx_row_normalize`, `fx_row_tail`) are
context-owned and remain valid until context destruction.

The transient allocations that would benefit most from a scratch region are
produced by `fx__row_normalize`, which is used by **both** internal solver
processors (where the result is genuinely transient and never escapes) and by
public query paths (where the result is returned to the caller and MUST remain
valid). Routing only the internal uses to a resettable scratch requires a
per-call choice of allocator that is easy to get wrong; a single misclassified
path would free a row the caller still reads, violating the ownership contract
and the green acceptance rules.

`fx_solve` also creates persistent objects (bindings via `make_row`, facts,
watchers) that share the same allocator, so a coarse "reset at end of solve"
cannot distinguish them from true temporaries without restructuring.

## What would be needed (future)

- A narrowly scoped scratch region used only by allocations that are provably
  never returned and never stored (per-constraint normalized operand copies,
  set-difference arrays, JOIN support accumulation), with the public/query
  normalize paths left on the main arena.
- A documented ownership rule distinguishing "internal temporary" from
  "context-owned, valid until free".
- Property tests confirming no returned or stored object is ever allocated
  from the resettable region.

Until then the arena-growth cost is accepted as a known characteristic of the
context-monotonic ownership model.
