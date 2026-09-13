# librepl89 design — event-driven example

## Purpose

`examples/echo_events.c` is the event-driven counterpart of `echo.c`. It shows
the smallest client that handles terminal resizing promptly: the application,
not the library, owns the signal policy, and the event loop turns a signal
into a redisplay request.

## Shape

- `main` installs a `SIGWINCH` handler with `sigaction` and no `SA_RESTART`,
  then runs the loop. The handler only sets `volatile sig_atomic_t resized`.
- `echo_loop` starts a session, observes the flag, calls `repl89_feed`, and
  applies the reported event. A resize redraws immediately because the
  signal interrupts the blocking read, `feed` converts `EINTR` into a
  successful `NONE` return, and the loop then calls `repl89_resize`.
- On `SUBMIT` the loop calls `repl89_submit` before echoing, so the terminal
  is restored and application output is newline-translated. On `CANCEL` it
  calls `repl89_cancel` before printing `^C`. EOF ends the loop with status 0.
- `repl89_free` on every exit path restores termios.

## Why the library does not own SIGWINCH

A global handler inside a small library would replace any handler the host
application installed and would hide policy in library state. Keeping the
handler in the client leaves the library free of callbacks and signals while
still allowing immediate redisplay in an event loop. The blocking
`repl89_read` convenience cannot react to a signal by itself; its contract is
documented as handling a resize on the next interaction.
