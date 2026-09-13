# 0003 - Design: Salson update rules -- confirmed ground truth & falsification

Status: **investigation complete, implementation deferred.**
Companion to `0002-dynamic-design.md`. Records what is *known and falsified*
about how a single-character edit reorders the maintained transform, before
any incremental update rule is adopted. The exact per-edit cell/rank update
rules are **still open** (see `0002`), but the false starts are now documented
so a future session does not re-derive them from scratch.

## Confirmed representation (matches src/bwt89_bwt.c, src/bwt89_ibwt.c)

- Text `T[0..n-1]`; internal symbols `sym[i] = rank(T[i])` with
  `sym[n] = SENTINEL(0)`; length `N = n + 1`.
- Rows 0..N-1 are the rotations of `sym`, sorted lexicographically. The suffix
  array `sa[r]` is the physical start index of row r.
- `p = orig = row with sa == 0` (the full-text rotation). `L[r] = sym[(sa[r]-1) mod N]`,
  so `L[p] == SENTINEL`. The reported transform bytes `B` = the cells of `L`
  in row order with the cell at row `p` removed; the reported index is `p`.
- `bwt89_ibwt(B, n, p) == T` (LF round trip). Verified by the model and by
  `test_dyn.c`.

So the dynamic handle must maintain `(T, L, p)`; `B` is derived from `(L, p)`.

## Reverse-engineering tooling (git-ignored, preserved on disk)

Under `.agent/tmp/` (git-ignored by the root `.gitignore`):
- `model.py` -- authoritative Python model of the above semantics (SA via
  doubling, LF inverse), round-trips exactly; used to compute the oracle.
- `falsify.py` -- exhaustive single-delete / single-insert oracle comparators.
- `blocks.py`, `reorder.py` -- print old/new rotation order and count reorders.

A fresh session MUST re-run `model.py` sanity checks and the falsifiers there
before trusting any candidate rule. `.agent/tmp` is ephemeral; if it is lost,
re-derive from `0002` + this file (short).

## Falsified hypothesis (do NOT implement)

> Naive splice: deleting text char at `m` = remove the single `L` cell that
> holds `T[m]`, then shift the remaining cells and adjust `p` down by one.

**Refuted.** Deleting a character reorders the surviving rotations. The
survivor order is NOT "old row order minus the deleted row" in general.

Counterexample `T = [0,0,1]`, delete `m=2`:
- surviving physical starts `{0,1}` compare as `[1,1,2,0]` vs `[1,2,0,1]`
  (rotation0 < rotation1) before deletion, but as `[1,1,0]` vs `[1,0,1]`
  (rotation1 < rotation0) after deletion -- the two survivors swap.

Measured frequency of "some survivor reorder" under an exact oracle:
- binary: n=3 1/24, n=4 6/64, n=5 25/160, n=6 85/384;
- ternary: n=4 28/324, n=6 870/4374.

So reordering is common and grows with n and alphabet size. A correct update
MUST re-derive the affected row order; it is a genuine Salson-style LF/rank
localized update, not a column splice. See `0002` D3 / acceptance `0002`.

## Structural observation (candidate, to be confirmed by falsification)

In the observed counterexamples the reordering is confined to the bucket of
rotations that share the symbol immediately preceding the edit in the circular
text (the LF/`less`+`occ` bucket of `sym[m-1]`). This is consistent with the
Salson description in `0002` but is NOT yet proven across the exhaustive
domain; it is a lead for the deriving session, not an accepted rule.

## Confirmed theorem (this session): delete reorder is a contiguous block resort

Empirically verified with **zero violations** over the exhaustive domain
(binary `n <= 8`, ternary `n <= 5`, every text, every delete position `m`),
using `model.bwt` as oracle:

> Deleting text char at index `m` reorders ONLY a contiguous block `[lo..hi]`
> of the surviving rows (in old row order). Rows strictly before `lo` and
> strictly after `hi` keep both their relative AND absolute order. Equivalently
> the new row order equals the old survivor order with the single interval
> `[lo..hi]` re-sorted.

Consequences:
- A correct DELETE is: remove the deleted occurrence's L cell, then reorder the
  interval `[lo..hi]`; the prefix and suffix are untouched by construction.
- The interval is typically small but has worst case ~ `n-1`. Measured
  interval sizes (delete): binary `n<=8` avg `~2-3`, max `n-1`; ternary `n<=5`
  avg `~2`, max `n-1`. Prefix+suffix preservation held in 100% of cases.

Still open (do not assume solved): how to compute `lo`,`hi` and the corrected
block order in `O(occ)` / sublinear time from maintained `(T,L,p)` + LF, and
how INSERT (the inverse) and SUBSTITUTE express the same block move. The
block-resort-by-text-comparison method is NOT acceptable as a general Salson
rule because its worst case is a full re-sort (violates D3 / decision 7); it is
only a bounded, correct baseline when the interval is provably small. The
sublinear derivation is the outstanding research item.

## Next steps for the deriving session (after this scaffold)

1. In `.agent/tmp`, derive and falsify `delete(T,m)` maintaining `(T,L,p)`:
   remove the row for start `m`, recompute the affected bucket's order, renumber
   `p`, and update `freq/less/occ`. Gate: exhaustive single-delete equality with
   `model.bwt` (Probe 1) over binary length <= 8 and ternary length <= 5.
2. Derive `insert(T,m,c)` as the inverse, same gate.
3. Derive `substitute = delete then insert`, same gate.
4. Port the proven rules into `src/bwt89_dyn.c`, populate `(T,L,p)` + LF tables
   at open, and make `ed_bwt` read maintained state so `test_dyn_incremental.c`
   (no `bwt89_bwt`/`bwt89_sa_is`) goes green. Keep the exhaustive oracle test
   `test_dyn_falsify.c` green throughout.
5. Then the Probe-4 LF-counter audit, fault-injection (H06-H10), memory safety,
   merge to `main`.

Green scaffold delivered now: `test/unit/test_dyn_falsify.c` (exhaustive
single-edit equality vs recompute) is committed green and will gate the above.
