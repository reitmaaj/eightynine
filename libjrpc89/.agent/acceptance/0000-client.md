# Acceptance: JSON-RPC 2.0 client

## Mandatory behaviors (must exhibit)

- `jrpc89_request_new` yields a request object with `jsonrpc: "2.0"`, the
  method, the params, and the id (integer, string, or null).
- A notification built with `JRPC89_ID_NONE` has no `id` member.
- `jrpc89_response_decode` accepts a valid response and produces a view whose
  kind distinguishes a result from an error; on `JRPC89_OK` every field
  permitted by the kind is usable without further structural checks.
- `jrpc89_response_decode` leaves `*out` unchanged on every non-OK return.
- `jrpc89_id_equal` compares ids structurally, including embedded NUL bytes.
- `jrpc89_fd_write_frame` and `jrpc89_fd_read_frame` round-trip a JSON message as
  newline-delimited bytes over an fd.
- The CLI demo connects to a Unix socket, sends a request, and prints the
  result or a structured error, exiting 0 on a successful call.
- `jrpc89_error_code_reserved` classifies the `-32768..-32000` range (including
  currently unassigned gaps) and the standard codes as reserved.
- Decoded error codes are exact `j89_int` values, preserving values outside
  the C `int` range that libj89 accepts.

## Unacceptable behaviors (must reject / refuse)

- A response whose `jsonrpc` member is not `"2.0"` MUST be rejected.
- A response carrying both `result` and `error` MUST be rejected.
- A response carrying neither `result` nor `error` MUST be rejected.
- A response without an `id` member MUST be rejected.
- A response whose `id` is a boolean, float, array, or object MUST be rejected;
  such ids MUST NOT be silently coerced to a null id.
- `jrpc89_request_new` MUST NOT return a usable object when any libj89 builder
  operation failed (for example, invalid UTF-8 in the method): the arena is
  marked failed and no node is returned.
- `jrpc89_request_new` MUST refuse an arena that is already marked failed or
  carries a pending error message.
- `jrpc89_error_code_reserved` MUST classify every code in `-32768..-32000` as
  reserved, including gaps such as `-32100` and `-32500`.
- A response whose `id` does not match the request id MUST be reported as a
  mismatch.
- The CLI demo MUST reject a response whose `id` does not match the request
  id instead of printing the result as a success.
- `jrpc89_request_new` MUST refuse a NULL id pointer and an unknown id kind.
- `jrpc89_request_new` MUST refuse a string id whose pointer is NULL and an
  integer id that is NaN, infinite, fractional, or outside ±2^53.
- `jrpc89_request_new` MUST refuse a NULL method pointer with a nonzero
  length without dereferencing it.
- A request `params` node that is present MUST be an array or an object;
  scalar params MUST be refused.
- A zero-length method name MUST be accepted and rendered as an empty JSON
  string.
- A response whose `error` member is not an object MUST be rejected.
- A response whose `error` object lacks an integer `code` or a string
  `message` MUST be rejected.
- `jrpc89_fd_read_frame` MUST accept a frame of up to `cap-1` payload bytes, MUST
  return `JRPC89_ETOOLONG` for a frame that exceeds the buffer and drain
  through the next newline so the following call starts on a frame boundary,
  MUST return `JRPC89_EOF` for EOF before any byte, and MUST return
  `JRPC89_ETRUNC` for EOF inside a frame.
- `jrpc89_fd_read_frame` MUST set `*out_len` to zero and `buf[0]` to `'\0'` on
  every non-OK return (except `JRPC89_EINVAL`, which touches nothing).
- `jrpc89_fd_write_frame` MUST refuse input containing a raw `'\n'` or a
  zero-length frame with `JRPC89_EINVAL`, writing nothing.
- The CLI MUST reject a `params` argument that is not valid JSON by reporting
  an error and exiting nonzero, rather than silently omitting params.
- The e2e suite MUST cover at least 100 passing exchanges (valid responses,
  exit 0, result echoed) and at least 100 failing exchanges (valid server
  errors reported as a structured error, or malformed/framing/mismatch/request
  failures rejected with a nonzero exit).
- An integer id outside libj89's exact range (|n| > 2^53) MUST be rejected
  by libj89 rather than approximated.
- The library MUST NOT connect, accept, or close sockets; it only uses the
  provided fd.
- The framing logic MUST NOT depend on POSIX headers; read and write are
  isolated behind the adapter in `src/io_posix.c`.
- A write interrupted by EINTR MUST be retried, and a short write MUST be
  completed by looping.
- The CLI MUST survive a write to a pipe whose read end is closed (no
  SIGPIPE termination) and report the failure with a nonzero exit.
- The full unit, fault, and e2e suites MUST run clean under AddressSanitizer
  and UndefinedBehaviorSanitizer via `just sanitize`.
