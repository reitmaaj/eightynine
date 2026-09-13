# 0002 - Design: dynamic (Salson) incremental-edit BWT

Status: **partially implemented (correctness-first baseline on `feat/dynamic`).**
Home branch `feat/dynamic`. Not merged to `main`.

Implemented so far: a correct dynamic editor that stores the current text and
mutates it with localized byte moves on each edit; the transform view
(`bwt89_ed_bwt`) is derived from that text on demand and is verified to equal a
fresh recompute after every edit. The genuinely sublinear, LF/rank-localized
maintenance of the transform itself (Salson et al.) is the **outstanding
algorithmic milestone** -- not yet implemented.

## Goal

Maintain, under an arbitrary sequence of single-character text edits
(insert / delete / substitute at any position), a Burrows-Wheeler transform
that always equals a freshly recomputed regular transform of the current text,
without re-running full suffix-array construction after every edit.

## Data model and public surface

The regular `bwt89_bwt` reports the row `index` of the original text, so a
maintained transform must track both the BWT byte string **and** that `index`.
The dynamic handle therefore maintains a triple that stays in sync:

    (text T, bwt bytes B, primary index p)

such that after every edit `bwt89_ed_bwt(e)` yields exactly the `(B, p)` that
`bwt89_bwt` would produce for the current text, and
`bwt89_ibwt(B, n, p) == T`.

Public entry points live in the `bwt89_` namespace (strict ISO C89, header
`include/bwt89.h`):

```c
struct bwt89_ed;                                        /* opaque handle */

enum bwt89_status bwt89_ed_open(struct bwt89_ed **ed,
                                const unsigned char *s, size_t n);
enum bwt89_status bwt89_ed_insert(struct bwt89_ed *ed,
                                  size_t pos, unsigned char c);
enum bwt89_status bwt89_ed_delete(struct bwt89_ed *ed, size_t pos);
enum bwt89_status bwt89_ed_substitute(struct bwt89_ed *ed,
                                      size_t pos, unsigned char c);
enum bwt89_status bwt89_ed_length(const struct bwt89_ed *ed, size_t *len);
enum bwt89_status bwt89_ed_bwt(const struct bwt89_ed *ed,
                               size_t *index, unsigned char *out);
enum bwt89_status bwt89_ed_close(struct bwt89_ed *ed);
```

A dynamic transform necessarily owns persistent state, so the handle is
heap-allocated by the library (unlike the stateless one-shot transforms).
Handle creation reports `BWT89_NOMEM` when the allocation fails. Edits that
would be out of bounds, an edit against a closed handle, or an operation the
structure cannot legally express return `BWT89_BAD_EDIT`. A `NULL` required
pointer returns `BWT89_NULL_ARG`. No owned type name ends in `_t`.

`pos` ranges over `[0, len]`: `insert` may target `pos == len` (append) and is
rejected if `pos > len`; `delete` and `substitute` require `pos < len`;
`delete` on an already-empty handle is `BWT89_BAD_EDIT`.

## Incremental update approach

**Implemented (correctness baseline):** the handle owns the text store and its
capacity. `insert`/`delete`/`substitute` validate the edit, then apply a single
localized byte move to the stored text (`open_slot` / `close_slot`, backed by
`memmove`). No suffix array or BWT is rebuilt at edit time. `bwt89_ed_bwt`
materializes the current transform by delegating to `bwt89_bwt` over the stored
text; because that delegate is exactly the recompute oracle, the maintained view
is correct by construction and is pinned by `test_dyn.c`.

**Milestone (Salson-incremental; spec only, implementation deferred).** The
goal is to stop deriving the transform on demand and instead carry the
transform incrementally so a single edit repairs only the affected LF/rank
cells. This is research-grade (the handover flagged it as such); no update rule
is committed until it is falsified against the recompute oracle below. The
remainder of this section is the concrete specification a future session must
implement.

### Maintained state (extension of the baseline handle)

In addition to the stored text `T` (length `n`), carry:

- `B[0..n-1]` -- the current transform bytes, always equal to `bwt89_bwt(T)`
  output.
- `p` in `[0, n]` -- the primary index, equal to the `index` `bwt89_bwt`
  reports.
- An LF/rank structure over the sentinel-augmented last column so that, given
  a text position, the module can locate the matching row in the transform and
  vice-versa. Concretely: the length-(n+1) last column `L` of the rotation sort
  of `T` with the appended sentinel (this is exactly the array inside
  `bwt89_ibwt`: `L[index]=SENTINEL`, others are ranked bytes), together with
  per-symbol occurrence counters (`freq`, `less`, and the running occurrence
  `occ`) as in `src/bwt89_bwt.c`.

The one global invariant that every operation MUST preserve, and the only one a
correctness test is allowed to trust:

> **I.** After every edit `bwt89_ed_bwt(e)` equals `(B, p)` and that pair
> equals a fresh `bwt89_bwt(T)`; hence `bwt89_ibwt(B, n, p) == T`.

A handle may only be in the "after an edit" state that satisfies I; a rejected
edit leaves `(T, B, p)` exactly as before (I still holds).

### Intended per-edit update (to be confirmed by falsification)

An edit at text position `k` changes exactly one byte of `T` at a known index.
Map that text byte to its row in the transform by the symbol class of the byte
immediately before it under the rotation ordering (the row whose last-column
symbol precedes `T[k]` in the text). Candidate rule set, each item is an OPEN
decision to be pinned:

1. **delete(k):** remove `T[k]`. Locate the row whose removal corresponds to
   excising that byte; delete its cell from `L`/`B`, decrement the occurrence
   counters for its symbol class, and decrement every LF pointer whose target
   lay after the removed row. Renumber `p` so I still holds.
2. **insert(k, c):** insert `c` into `T` before `T[k]`. Add one cell of symbol
   `c` to `L`/`B` in the correct bucket of the symbol's occurrence interval,
   increment the counters for `c`, and increment every LF pointer whose target
   lay at or after the spliced row. Adjust `p`.
3. **substitute(k, c):** `delete(k)` then `insert(k, c)` (same position), or an
   atomic swap of one cell's symbol if the two-step rule is shown equivalent.

Rules 1-2 are stated as a *target*, NOT as proven correct. The subtle parts --
exactly which row corresponds to text position `k`, and precisely which LF
pointers shift when a cell is added or removed -- are the open research
decisions. Each candidate formulation MUST be rejected or adopted by the
deterministic probes in the next section before it may be committed.

### Complexity budget (acceptance D3)

A per-edit update must touch `O(occ(c))` LF cells for the edited symbol (or
better), i.e., it MUST NOT re-derive the suffix array / re-sort the whole text.
Worst case must remain below a full `O(n)` transform derivation for the edited
class. If the occurrence-heavy worst case equals `Theta(n)` this is acceptable
only if the average/canonical edit is cheaper; otherwise the module does not
satisfy D3 and must not claim it does.

### Validation / falsification protocol (run before adopting any update rule)

Implement `src/bwt89_dyn.c` so that `bwt89_ed_bwt` reads the maintained
`(B, p)` directly (no call to `bwt89_bwt` inside the hot edit/export path is
allowed once the milestone is reached), then run, over exhaustive small inputs
and thousands of randomized/structured edit sequences:

- **Probe 1 (equality):** after each edit, `bwt89_ed_bwt == bwt89_bwt(T)` byte
  for byte and with equal `p`. Any mismatch falsifies the adopted rule.
- **Probe 2 (round-trip):** `bwt89_ibwt(B, n, p) == T`.
- **Probe 3 (rejection hygiene):** after each rejected edit (out-of-range,
  delete-on-empty, NULL args) the state still equals recompute of the unchanged
  text.
- **Probe 4 (invariant audit):** cross-check the LF counters (`freq`, `less`,
  running `occ`) against a from-scratch recount of `B`/`L` after every edit, to
  catch a drift that Probe 1 would also flag but with a sharper location.
- **Probe 5 (no-recompute guard):** an instrumentation pass over the edit and
  export functions asserts that the maintained `(B, p)` is not produced by an
  internal call to `bwt89_bwt`/`bwt89_sa_is` on the whole text. Until Probe 5
  passes, D3 is not met.

Only after all five probes pass on the exhaustive+randomized suite may the
module be considered Salson-incremental and the acceptance D3 line in
`.agent/acceptance/0002` be moved from "not met" to "met".

### Reference

Salson, Lecroq, Leonard and Mouchard, "A four-stage algorithm for updating a
Burrows-Wheeler transform" (2010), and Salson et al., "Dynamic extended BWT"
(2009). The current module's own `src/bwt89_bwt.c` is the authoritative
definition of the sentinel representation that the maintained state must match.

## Module boundaries

| Translation unit | Responsibility |
|------------------|----------------|
| `src/bwt89_dyn.c` | The handle type, its LF/rank tables, and the four edit operations. |
| `include/bwt89.h` | Adds the `bwt89_ed_*` declarations and reuses `BWT89_BAD_EDIT`. |

Reuses the LF-count/prefix idioms already proven in `src/bwt89_bwt.c`. Written
to the green worker/controller shape from the start (many small 2-7-line
workers; thin decision/iteration bodies delegating in single glue calls).

## Correctness oracle (implementation-independent)

- After **every** edit, `bwt89_ed_bwt` MUST equal a fresh `bwt89_bwt` of the
  current text (same bytes and same `index`), and `bwt89_ibwt` of that
  maintained transform MUST reproduce the current text.
- Must-reject edits never corrupt the handle: after a rejected edit the
  maintained transform still equals the recompute of the unchanged text.
