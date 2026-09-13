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
  and the `-32000..-32099` server range; reserved-vs-application
  classification; error-object access.
- Response validation: an `id` must be present and match the request id;
  exactly one of `result` or `error` must be present; an `error` member must
  be an object carrying an integer `code` and a string `message`.
- Request building refuses an empty or NULL method.
- NDJSON framing over an already-open Unix socket file descriptor, with
  distinct return codes for end-of-file, frame-too-long, read error, and
  truncated frame.
- A CLI demo that sends one request on a provided fd and prints the result
  or a structured error; it rejects invalid `params` JSON (rather than
  silently omitting params) and rejects a mismatched response id.

No batching in V1.

## Transport contract

The library operates on an **already-open Unix socket `int fd`**. It does
not `connect`, `accept`, or manage socket lifecycle. Frames are
newline-delimited JSON (NDJSON): one message per line. The CLI demo reads
into an 8192-byte buffer, so a response frame may be up to 8191 bytes; a
larger frame is reported as too long.

## Known limitation: error code range

An error `code` is validated as a JSON integer but is **not range-checked
against `int`**. A code within libj89's `±2^53` exact-integer range yet
outside `INT_MIN..INT_MAX` is accepted as valid and then truncated when it
is cast to `int` for classification and printing (`src/error.c`,
`jrpc89_error_code`). Callers should keep error codes within the `int`
range.

## Build and test

```sh
just build       # compile the jrpc89 CLI (library + libj89)
just test        # smoke, unit, e2e scripts, and the parameterized suite
just e2e-suite   # parameterized exchange suite only
just green       # generate compile DBs and run the seven-cell green matrix
just check       # green gate (matrix + tidy + format)
just lint        # shellcheck + clang-format dry-run
just format      # apply canonical formatting
```

## Test suites

`just test` runs, in coverage order:

1. **smoke** — one end-to-end request/response round trip over a Unix socket.
2. **unit** — one test binary per library module (`test/unit/test_*.c`).
3. **e2e scripts** — `smoke.sh`, `error.sh`, `mismatch.sh` driven by the
   Python mock server.
4. **e2e-suite** — a parameterized matrix (`test/e2e/suite.py`) that must
   cover at least 100 passing exchanges (valid responses, exit 0, result
   echoed) and at least 100 failing exchanges (valid server errors reported
   as a structured error, or malformed/framing/mismatch/request failures
   rejected with a nonzero exit). It currently runs 140 passing and 134
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
include/jrpc89.h        public API
src/                    library modules + main.c (CLI)
test/unit/              unit tests
test/e2e/               mock server + harness + parameterized exchange suite
.agent/                 concept, stories, design, testing, acceptance
```

## Dependency

`libj89` (`../libj89`) provides parsing, the node tree, the builder API,
and rendering. It is compiled from source as part of the build.

## Development discipline

Every behavior change follows: scenario in `.agent/testing/*.md` →
acceptance in `.agent/acceptance/*.md` → failing test → minimum code →
refactor. Defect fixes are hypothesis-driven with a reproduction test
before any code change. See `.agent/` and `AGENTS.md`.
