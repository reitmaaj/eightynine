# librepl89 acceptance tests — fault injection and invariants

## 0016 — Unacceptable behavior (must reject / fail safely)

- FI-01. `tcgetattr` failure MUST return `REPL89_ETTY` and leave the session
  inactive.
- FI-02. `tcsetattr` failure MUST return `REPL89_ETTY` and leave the session
  inactive.
- FI-03. `TIOCGWINSZ` failure MUST return `REPL89_EIO`; a later start MUST
  still succeed, proving cleanup.
- FI-04. `read` failure MUST return `REPL89_EIO`; the session MUST remain
  recoverable (cancel restores).
- FI-05. `write` failure during session start MUST return `REPL89_EIO` and
  leave the session inactive.
- FI-06. Allocation failure in `repl89_new` MUST return `NULL`.
- FI-07. Allocation failure in `repl89_insert` MUST return `REPL89_ENOMEM`
  with the buffer and cursor unchanged.
- FI-08. A malformed paste MUST return `REPL89_EUTF8` with the buffer
  unchanged; the session MUST remain usable.
- FI-09. No memory leak or double free may occur on any of the above paths
  (valgrind).
- FI-10. A `write` interrupted by `EINTR` MUST be retried; the bytes MUST
  still be delivered. Other write errors MUST remain errors.
- FI-11. A write failure during a cursor-only or content update MUST be
  reported to the caller; the session MUST remain recoverable.
- FI-12. A write failure while clearing a hidden region MUST be reported; a
  later show MUST redraw successfully.
- FI-13. A write failure during submit finalization MUST be reported while
  the terminal configuration is still restored and the session ends.

## 0017 — Required invariants (must hold)

- R89-I1. The edit buffer always contains valid UTF-8 whose only controls
  are LF and HT, after any sequence of operations.
- R89-I2. The cursor always lies on a grapheme boundary (or at the end).
- R89-I3. No operation splits a UTF-8 sequence or a grapheme cluster.
- R89-I9. Rendering never changes the logical submission.
