# libjrpc89 — a green-compliant C JSON-RPC 2.0 protocol core

`libjrpc89` is a strict-ISO-C89 protocol core library for JSON-RPC 2.0. It
builds and decodes request and notification objects, builds and parses
response and error objects, and moves frames over an already-open connected
stream-socket file descriptor. All JSON processing is delegated to the
sibling `libj89` library.

## Positioning

The library is a thin, transport-agnostic JSON-RPC 2.0 message layer. It
does not manage sockets, TLS, batching, or the transport lifecycle; it
assumes an opened, blocking, connected stream-socket file descriptor is
provided and reads and writes newline-delimited JSON frames on it. The
protocol core is ISO C89; the optional framing profile in `jrpc89_io.h` is
POSIX-specific.

## Why strict C89 + green

The project inhabits the strict `C89 ∩ C23` intersection under both GCC and
Clang with explicit semantic structure and one canonical Allman format. This
is the sibling `green` profile. The discipline yields long-lived, portable,
inspectable C source where every effect, mutation, and control decision is
explicit statement-level structure.

## Scope (V1)

- Core objects: request, notification, response, error.
- `jsonrpc: "2.0"` field on every message.
- `method` and `params` on requests.
- `id` echoed back for string, integer, and null ids.
- Notifications omit `id`; no response is expected.
- Request decoding into a checked `jrpc89_request` view.
- Response construction for results and errors.
- Standard error codes and a checked result-or-error decode view.
- NDJSON framing over a caller-provided open stream-socket fd.
- A CLI demo that drives the library over a provided fd.

## Out of scope (V1)

- Batching (arrays of requests/responses in one message).
- Socket connection/accept lifecycle in the library.
- TLS, authentication, transport retries.
- Server-side dispatch / method handlers.
