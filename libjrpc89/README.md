# libjrpc89

A **green-compliant JSON-RPC 2.0 client** library and CLI demo in strict
ISO C89, using the sibling `libj89` project for all JSON processing.

`green` compliance means every translation unit passes the seven-cell
matrix — GCC C89, GCC C23, Clang C89, Clang C23, clang-tidy C89,
clang-tidy C23, and canonical formatting.

## Features (V1)

- Request, notification, response, and error objects.
- `jsonrpc: "2.0"` on every message.
- String, integer, and null `id`s echoed back; notifications omit `id`.
- Standard error codes `-32700`, `-32600`, `-32601`, `-32602`, `-32603`,
  and the full reserved range `-32768..-32000`; reserved-vs-application
  classification; error-object access. Error codes are exact `j89_int`
  values, so codes outside the C `int` range are preserved.
- Response decoding into a checked result-or-error view: an `id` must be
  present and be an integer, string, or null; exactly one of `result` or
  `error` must be present; an `error` member must be an object carrying an
  integer `code` and a string `message`. On success every field permitted by
  the kind is usable without further structural checks.
- A single `jrpc89_status` namespace for protocol and transport results, and
  a strict request contract: the arena must be clean, and `*out` is unchanged
  on failure.
- Request building takes the method as bytes plus an explicit length, so
  embedded NUL bytes are preserved and zero-length methods are allowed.
  `params`, when present, must be an array or object.
- NDJSON framing over an already-open Unix socket file descriptor, with typed
  statuses for EOF, truncation, oversized frames, and I/O failure. An
  oversized frame is drained through its newline so the next read starts on a
  frame boundary, and failed reads clear the output state.
- A CLI demo that sends one request on a provided fd and prints the result
  or a structured error; it rejects invalid `params` JSON (rather than
  silently omitting params) and rejects a mismatched response id.

No batching in V1.

## Public API (V1)

```text
jrpc89_request_new        build a request/notification node
jrpc89_response_decode    decode a parsed response into jrpc89_response
jrpc89_id_equal           structural id equality
jrpc89_error_code_reserved  reserved interval -32768..-32000
jrpc89_fd_write_frame    write one NDJSON frame
jrpc89_fd_read_frame     read one NDJSON frame
```

Types: `jrpc89_status`, `jrpc89_id`, `jrpc89_response`, `jrpc89_error`.
The protocol core is ISO C89 and POSIX-free; the framing functions are the
POSIX transport profile.

## Transport contract

The library operates on an **already-open, blocking, connected Unix socket
`int fd`**. It does not `connect`, `accept`, or manage socket lifecycle.
Frames are newline-delimited JSON (NDJSON): one message per line. The CLI
demo reads into an 8192-byte buffer, so a response frame may be up to 8191
bytes; a larger frame is drained and reported as too long, leaving the
stream on the next frame boundary. The adapter suppresses SIGPIPE where the
platform offers `MSG_NOSIGNAL`; elsewhere the application must configure its
SIGPIPE policy (the CLI ignores it). Nonblocking descriptors are out of
scope in V1.

## Known limitation: none

Error `code` values are validated as JSON integers and preserved as exact
`j89_int` values; classification covers the complete reserved interval
`-32768..-32000`. Request construction propagates every libj89 builder
failure and never returns a node from a failed construction.

## Build and test

```sh
just build       # compile the jrpc89 CLI (tool + library + libj89)
just test        # smoke, unit, fault, e2e scripts, and the parameterized suite
just fault       # scripted syscall-seam framing tests only
just e2e-suite   # parameterized exchange suite only
just green       # generate compile DBs and run the seven-cell green matrix
just check       # green gate (matrix + tidy + format)
just sanitize    # ASan/UBSan build of the library and tests
just ci          # test + check + sanitize
just lint        # shellcheck + clang-format dry-run
just format      # apply canonical formatting
```

## Test suites

`just test` runs, in coverage order:

1. **smoke** — one end-to-end request/response round trip over a Unix socket.
2. **unit** — one test binary per library module (`test/unit/test_*.c`).
3. **fault** — framing tests against a scripted syscall seam
   (`test/fault/test_*.c`) covering EINTR, short transfers, EOF, and errors.
4. **e2e scripts** — `smoke.sh`, `error.sh`, `mismatch.sh`, and `sigpipe.sh`
   driven by the Python mock server.
5. **e2e-suite** — a parameterized matrix (`test/e2e/suite.py`) that must
   cover at least 100 passing exchanges (valid responses, exit 0, result
   echoed) and at least 100 failing exchanges (valid server errors reported
   as a structured error, or malformed/framing/mismatch/request failures
   rejected with a nonzero exit). It currently runs 124 passing and 150
   failing cases and fails unless every case passes.

## CLI demo

```sh
jrpc89 <fd> <method> [params-json]
```

`<fd>` must be an open, connected Unix socket. The CLI writes one request
and reads the matching response on that fd. `params-json`, when given, must
be valid JSON; otherwise the CLI reports an error and exits 1. The
mock-server harness in `test/e2e/mock_server.py` shows the intended driver
pattern.

## Layout

```text
include/jrpc89.h        protocol model/build/decode API (ISO C89)
include/jrpc89_io.h     optional POSIX fd + NDJSON profile
src/                    library modules (io.c framing, io_posix.c adapter)
tool/jrpc89.c           CLI demo
test/unit/              pure and protocol unit tests
test/fault/             scripted syscall-seam tests
test/e2e/               mock server + harness + exchange and SIGPIPE suites
.agent/                 concept, stories, design, testing, acceptance
```

## Dependency

`libj89` (`../libj89`) provides parsing, the node tree, the builder API, and
rendering. It is built as a static archive (`../libj89/build/libj89.a`) and
linked; libjrpc89 never compiles libj89 sources directly. libj89 in turn
links `libstr89` and `libu89`.

## Development discipline

Every behavior change follows: scenario in `.agent/testing/*.md` →
acceptance in `.agent/acceptance/*.md` → failing test → minimum code →
refactor. Defect fixes are hypothesis-driven with a reproduction test
before any code change. See `.agent/` and `AGENTS.md`.
