# librepl89 acceptance tests — echo example

## 0018 — Unacceptable behavior (must reject / fail safely)

- EX-05. A cancelled submission MUST NOT be emitted as an echoed line; only
  the editor's own rendering may show the typed text before cancellation.
- EX-06. An error from `repl89_read` MUST end the example with a non-zero exit
  status after `repl89_free` restores the terminal; the example MUST NOT hang
  or continue on a corrupt session.
- EX-07. The example MUST NOT interpret submission content: bytes are echoed
  verbatim, never evaluated, expanded, or rewritten.

## 0019 — Required behavior (must exhibit)

- EX-01. The example builds and runs via `just echo`.
- EX-02. A submission is echoed byte-for-byte followed by exactly one LF, so
  embedded LFs (Ctrl-J) and UTF-8 scalars survive the round trip.
- EX-03. After every submit or cancel the example prompts again with `> ` and
  keeps reading until EOF.
- EX-04. Ctrl-D on an empty submission exits with status 0 and restores the
  terminal; every submitted line is added to history so Up recalls it.
- EX-07. The event-driven example MUST install a SIGWINCH handler that only
  sets a flag (no library-owned signal policy), observe the flag in its loop,
  and call `resize` so a size change redraws without waiting for a keypress.
- EX-08. The event-driven example MUST end the session before writing echo or
  cancel output, so application output is written with termios restored.
- EX-09. The blocking example MUST remain the smallest client; resize
  handling for it stays documented as "on the next interaction".
