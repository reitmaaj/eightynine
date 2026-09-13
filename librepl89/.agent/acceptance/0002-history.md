# librepl89 acceptance tests — history

## 0003 — Unacceptable behavior (must reject / fail safely)

- HI-14. An allocation failure during `history_add` MUST leave the existing
  history unchanged and return `REPL89_ENOMEM`.
- HI-15. An invalid entry (forbidden control, malformed UTF-8) MUST be
  rejected atomically (`REPL89_EINVAL` / `REPL89_EUTF8`); history unchanged.
- HI-11. `submit` MUST NOT implicitly add history.
- HI-12. `cancel` MUST NOT alter history.
- HI-16. Two editor instances MUST NOT share history storage; freeing one
  MUST NOT affect the other.

## 0004 — Required behavior (must exhibit)

- HI-01. `history_limit == 0` disables retention: `history_add` succeeds and
  retains nothing.
- HI-02. Adding one entry retains exactly one entry, byte-identical.
- HI-03. Adding the same entry twice retains two entries (no hidden
  duplicate suppression).
- HI-04. A multiline entry is stored and restored byte-identically.
- HI-05. A UTF-8 entry is stored and restored byte-identically.
- HI-06. At the configured limit, adding evicts the oldest entry and retains
  the newest.
- HI-07. Backward navigation returns the newest entry first, then older ones,
  and stops at the oldest.
- HI-08. Forward navigation returns toward the newest entry and then reports
  the draft position.
- HI-09/HI-10. The unsubmitted draft remains recoverable when navigating
  forward past the newest entry (editor-level, exercised in the session
  increment).
- HI-13. History is instance-owned: two editors keep independent entries.
