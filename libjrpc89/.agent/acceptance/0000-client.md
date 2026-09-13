# Acceptance: JSON-RPC 2.0 client

## Mandatory behaviors (must exhibit)

- `jrpc89_request_new` yields a request object with `jsonrpc: "2.0"`, the
  method, the params, and the id (integer, string, or null).
- A notification built with `JRPC89_ID_NONE` has no `id` member.
- `jrpc89_response_validate` accepts a valid response and distinguishes a
  result from an error.
- `jrpc89_error_code`, `jrpc89_error_message`, and `jrpc89_error_data`
  extract the members of a parsed error object.
- `jrpc89_write_frame` and `jrpc89_read_frame` round-trip a JSON message as
  newline-delimited bytes over an fd.
- The CLI demo connects to a Unix socket, sends a request, and prints the
  result or a structured error, exiting 0 on a successful call.
- `jrpc89_error_is_reserved` classifies the `-32000..-32099` range and the
  standard codes as reserved.

## Unacceptable behaviors (must reject / refuse)

- A response whose `jsonrpc` member is not `"2.0"` MUST be rejected.
- A response carrying both `result` and `error` MUST be rejected.
- A response carrying neither `result` nor `error` MUST be rejected.
- A response without an `id` member MUST be rejected.
- A response whose `id` does not match the request id MUST be reported as a
  mismatch.
- The CLI demo MUST reject a response whose `id` does not match the request
  id instead of printing the result as a success.
- `jrpc89_request_new` MUST refuse an empty or NUL method name.
- `jrpc89_request_new` MUST refuse a NULL method pointer without dereferencing
  it.
- A response whose `error` member is not an object MUST be rejected.
- A response whose `error` object lacks an integer `code` or a string
  `message` MUST be rejected.
- `jrpc89_read_frame` MUST accept a frame of up to `cap-1` payload bytes, MUST
  report a frame that fills the buffer without a newline as too long, and MUST
  report a truncated frame (peer closed before a newline) distinctly from a
  too-long frame.
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
