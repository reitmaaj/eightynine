# librepl89 design — echo example

## Purpose

`examples/echo.c` is the smallest complete client of the public API: a
blocking REPL that echoes each submission. It makes the
start/feed/submit/cancel contract observable and is the starting point for
real REPLs.

## Shape

- `main` owns the process: it builds a `repl89_config` over stdin/stdout,
  creates the editor, runs the loop, frees the editor, and returns the loop's
  status.
- The loop uses `repl89_read`, the blocking convenience over the state
  machine; `SUBMIT`, `CANCEL`, `EOF`, and `ERROR` are control flow.
- A submission is written with `fwrite` plus one LF and then added to
  history; cancel prints `^C` and continues; EOF exits 0; error prints to
  stderr and exits 1.
- `repl89_free` on every exit path restores termios.

## Why blocking first

`repl89_read` hides the event loop and demonstrates the result contract with
the fewest moving parts. The event-driven API (`repl89_feed` plus
`hide`/`show`) is the subject of a later example once a client needs
interleaved output.
