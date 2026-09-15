# Acceptance criteria: libledger89 v2

Traceability: each criterion maps to scenarios in
`.agent/testing/0000-scenarios-v2.md` and tests under `test/`.

## Must exhibit

1. A created ledger reports `first == stable_end == end == 1` and
   `revision == 0`; identity is stable across reopen. (O01, O02)
2. `appendv` assigns consecutive positions and makes the batch logically
   visible atomically. (A01, A02)
3. `appendv_at` appends only when revision and end both match; otherwise
   `ESTALE` with no change. (A03)
4. `sync` advances `stable_end` to `end` and is a no-op on a clean ledger;
   recovery after power loss exposes exactly the stable prefix. (A04, C01)
5. `read` distinguishes `EGONE`, `ENOENT`, `ETOOSMALL`, and size-only
   queries, and never copies on a short buffer. (R01)
6. Iteration is ascending; `DONE` is not permanent; `truncate_from` yields
   `ESTALE`; pruning past the iterator yields `EGONE`. (R02, R03)
7. `truncate_from` requires a clean ledger, removes the suffix, increments
   `revision`, and is durable before returning. (S01, S02)
8. `prune_before` removes only whole sealed parts, reports
   `actual_first <= requested`, leaves `revision` unchanged, and is durable.
   (S03)
9. `rotate` seals the active part, preserves the sequence, and is a no-op
   when the active part is empty. (S04)
10. Recovery chooses the latest valid stable marker, discards the tail, and
    validates the retained prefix forward. (C02)
11. Every crash resolves to one published topology plus one stable prefix;
    repeated recovery is idempotent. (C04, C06)
12. A single injected I/O failure poisons mutations until reopen and the
    ledger remains recoverable and usable. (A05, F01)
13. Randomized operation sequences match a reference model exactly. (M01)
14. Frozen byte fixtures rebuild identically and decode correctly. (G01)
15. WAL-style, replicated-log-style, and checkpoint consumers are expressible
    with only the public API. (AB01)
16. `read_at` copies exactly the requested byte range of one record, treats
    `size == 0` as a valid no-copy query, and reports `EGONE`/`ENOENT` with the
    same conventions as `read`. (R04)
17. `read_at` verifies the whole-record checksum before reporting success for
    a non-empty copy, performs no allocation after open, and never mutates
    `revision`, `first`, `stable_end`, or `end`. (R05)
18. The Raft adapter suite reads term prefixes with `read_at(0, 8)` and
    payloads with `read_at(8, size - 8)`, maps a `raft89_proposev` batch to
    one `appendv_at` + `sync`, and preserves terms above 2^32. (AB01)
19. The library builds warning-free under the strict flag set on ILP32 and
    passes the full fast suite, the bespoke `--wrap` fault suites, and the
    Raft adapter under `-m32`.

## Must reject / fail safely

1. A second writable open, or a read-only open while a writer exists, returns
   `EBUSY`; invalid flag combinations return `EINVAL`. (O03, O05)
2. Mutations on a read-only handle return `EROFS`. (O04)
3. Zero-count batches, NULL slice arrays, NULL data with non-zero size, and
   payloads above the documented limit are rejected without state change.
   (A02)
4. `truncate_from` on a dirty ledger returns `EUNSTABLE`; out-of-range
   positions return `EGONE`/`ERANGE`. (S01)
5. `prune_before` past `stable_end` returns `EUNSTABLE`. (S03)
6. Corruption in stable history returns `ECORRUPT`; recovery never silently
   truncates it, and `CURRENT` never falls back to an older manifest. (C03,
   C05, X01)
7. A partial batch is never visible after any crash. (C01, C04)
8. A poisoned handle never reports success for a mutation. (A05)
9. Payload corruption is reported by `read`/`verify` rather than repaired.
   (G01, X01)
10. Indices below `first` return `EGONE`, never stale or invented data. (R01,
    R02)
11. `read_at` with `offset` past the record end, a range crossing the record
    end, or NULL data with non-zero size returns `ERANGE`/`EINVAL` and copies
    nothing. (R04)

## ILP32 verification (2026-09-15)

Host: Fedora 43, GCC 15.3.1, `cc -m32` multilib present.

- `just build32` — `build32: ok`. This exposed one real defect: the count
  range guards in `led89_validate_slices` and `led89_emit_batch` cast a
  32-bit `size_t` directly to `led89_u64`, which GCC rejects under
  `-Werror=type-limits` on ILP32. Both now widen `count` into a local
  `led89_u64` first, keeping the LP64 guard intact and warning-free.
- `just test32` — smoke, unit, api, crash, fault, `fault-nomem32`,
  `fault-eintr32`, corrupt, model, adapters, golden, and stress all pass
  under `-m32`.
- `just adapters-raft32` — the libraft89 + libledger89 fixture passes on
  ILP32 (envelope boundary, 4 KiB payload, `proposev` batch mapping, 64-bit
  term, index conversion).
- `just sanitize32` — the full fast suite under
  `-m32 -fsanitize=address,undefined`; passes on GCC 15.3.1 with the i686
  runtimes (`libasan.i686`, `libubsan.i686`) installed.

`just check` (green, lint, and the LP64 fast suite) remains the release gate;
the 32-bit recipes are additive and skip cleanly when the toolchain is absent.
