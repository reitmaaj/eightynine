# librepl89 acceptance tests — session state machine

## 0013 — Unacceptable behavior (must reject / fail safely)

- API-04. `feed` before `start` MUST return `REPL89_ESTATE` and MUST set
  `*event` to `NONE`; a stale event value from the caller MUST NOT survive
  the call.
- API-01. A second `start` while active MUST return `REPL89_ESTATE` and leave
  the session untouched.
- API-02/API-03. A second `submit` or `cancel` MUST return `REPL89_ESTATE`.
- API-05. `insert` after `submit` MUST return `REPL89_ESTATE`; the preserved
  submitted text MUST be unchanged.
- TY-04. `free` while active MUST restore the terminal configuration.
- Prompt validation MUST reject malformed UTF-8 (`REPL89_EUTF8`) and control
  scalars (`REPL89_EINVAL`) before entering the terminal.
- A malformed paste MUST leave the buffer unchanged and report
  `REPL89_EUTF8`.

## 0014 — Required behavior (must exhibit)

- ST-01. `submit` ends the session, restores the terminal, disables bracketed
  paste, and preserves the buffer and cursor.
- ST-04. `cancel` ends the session and clears the buffer.
- ST-05. Ctrl-D on an empty buffer reports `EOF` and ends the session.
- ST-06. Ctrl-D on a non-empty buffer deletes the next grapheme.
- ST-08. `start` after `submit` begins an empty submission.
- IO-07. Enter reports `SUBMIT` while leaving the session active until the
  caller invokes `submit`.
- QI-02/QI-03/QI-04. Bytes read after Enter stay queued and feed the next
  session without another read.
- QI-09. `feed` MUST redraw after processing queued bytes that changed the
  submission, even when no new input is available, so the region used by
  submit finalization is never stale.
- API-06/API-07. `text_get` returns the preserved submission after `submit`
  and empty text after `cancel`.
- EQ-01. `read` returns `SUBMIT`/`CANCEL`/`EOF`/`ERROR` matching the event
  interface on the same input.

## 0015 — Vertical movement and history fallback

- ST-10. Up/Down move to the adjacent visual row at the preferred column,
  including across soft wraps, not merely across LF.
- ST-11. The preferred column is preserved across short rows; horizontal
  movement resets it.
- ST-12. Up on the first visual row enters history backward; Down on the last
  visual row enters history forward (restoring the draft at the end).
- ST-13. Ctrl-P/Ctrl-N remain history-only and do not move vertically.

## 0016 — Resize and visibility (R7)

- RS-01. `resize` while inactive MUST return `REPL89_ESTATE`.
- RS-02. `resize` while active queries `TIOCGWINSZ` and fully redraws.
- API-09. `hide` while hidden MUST return `REPL89_ESTATE`.
- API-10. `show` while visible MUST return `REPL89_ESTATE`.
- hide/show MUST preserve the submission and cursor.
- `resize`/`hide`/`show` while inactive MUST return `REPL89_ESTATE`.

## 0020 — Display update classification

- ST-14. Cursor-only operations (Left, Right, Home, End, in-range Up/Down)
  MUST NOT emit printable submission bytes and MUST NOT emit erase-line or
  erase-screen sequences; only cursor-motion escapes may reach the terminal.
- ST-15. Operations that leave the submission and cursor unchanged MUST emit
  nothing.
- ST-16. Content-changing operations (insertion, deletion, newline, paste,
  history replacement) MUST repaint the region and leave the cursor at its
  new position.
- ST-17. A feed call that processes several keys MUST apply the strongest
  resulting update once; it MUST NOT redraw per key.
- ST-18. Failure of a text insertion during feed MUST report the allocation
  error and leave the submission unchanged.
- ST-19. A content change in an already-drawn session MUST repaint by
  overwriting the old region, never by erasing it first.
- RS-03. After shrinking the terminal, `resize` MUST reflow the old hard
  rows under the new width, repaint the new layout, and leave the cursor at
  the new layout's position.
- RS-04. After growing the terminal, `resize` MUST repaint the new layout
  and erase rows that only the old rendering occupied.
- RS-05. `resize` MUST NOT interpret the old region's recorded cursor row
  under the new geometry; reconstruction through reflow is required.
- RS-06. `resize` with unchanged dimensions MUST emit nothing and return
  `REPL89_OK`.
