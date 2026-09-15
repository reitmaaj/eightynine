# Acceptance: JSON-RPC 2.0 protocol core

## Mandatory behaviors (must exhibit)

- `jrpc89_request_decode` accepts a valid request or notification and fills
  every field: `method` bytes plus length, `params` node or `J89_BAD`, and an
  `id` view whose kind is `NONE` for notifications.
- `params`, when present, is an array or an object; `id`, when present, is an
  integer, a string, or null.
- Unknown additional members do not affect decoding.
- `jrpc89_response_result_new` and `jrpc89_response_error_new` build objects
  that `jrpc89_response_decode` accepts, preserving the id, result node, code,
  message bytes, message length, and optional data node exactly.
- The error builder omits the `data` member when `data == J89_BAD`.
- Embedded NUL bytes in methods, string ids, and error messages are preserved
  by exact length, never by `strlen`.
- `*out` is unchanged on every non-OK return from a builder or decoder.
- Error codes are exact `j89_int` values, including values outside the C `int`
  range.
- The protocol core builds warning-free on ILP32 and passes the whole suite
  (smoke, unit, fault, allocation-failure, and e2e) under `-m32`, with the
  unit, fault, and allocation-failure suites also clean under 32-bit
  ASan + UBSan (`just sanitize32`, GCC i686 runtime).

## Unacceptable behaviors (must reject / refuse)

- A request whose root is not an object, whose `jsonrpc` member is missing or
  not `"2.0"`, or whose `method` member is missing or not a string MUST be
  rejected with `JRPC89_EPROTO`.
- A request whose `params` member is a scalar MUST be rejected with
  `JRPC89_EPROTO`.
- A request whose `id` member is a boolean, float, array, or object MUST be
  rejected with `JRPC89_EPROTO`.
- A JSON text carrying duplicate protocol-significant members MUST be rejected
  by libj89's parser before jrpc89 sees a node.
- A response builder MUST refuse `JRPC89_ID_NONE`, a NULL id pointer, a NULL
  arena, a NULL `out`, a dirty arena, or an unknown id kind with
  `JRPC89_EINVAL`.
- A NULL error message with a nonzero length MUST be refused with
  `JRPC89_EINVAL` without dereferencing the pointer.
- A non-exact error code (NaN, infinite, fractional, or outside ±2^53) MUST be
  refused with `JRPC89_EINVAL`.
- An invalid result or data node index MUST be refused with `JRPC89_EINVAL`.
- A builder MUST NOT return a usable node when any libj89 builder operation
  failed: the arena is marked failed and no node is produced.
- A deterministic allocation failure at any allocation point MUST return
  `JRPC89_ENOMEM`, leave `*out` unchanged, and mark the arena failed without
  exposing a partially built node.
