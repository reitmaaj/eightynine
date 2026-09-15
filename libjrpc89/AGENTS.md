# libjrpc89 — agentic workflow

A green-compliant JSON-RPC 2.0 protocol core library and CLI demo written in
strict ISO C89, using the sibling `libj89` project for all JSON processing.
`libj89` delegates Unicode scalar/UTF-8/UTF-16 facts to the sibling `libu89`
and owns strings and keys through the sibling `libstr89`, so builds link all
three (`just deps` builds `libstr89`, which builds `libu89`). It is a sibling
git repository.

## Hard constraints

- **Green-compliant** (sibling `green` toolchain): every `.c` is ISO C89,
  clean under the strict C89∩C23 baseline for both GCC and Clang, passes the
  `green` clang-tidy semantic suite in both standards, and is canonically
  formatted (Allman). The conformance gate is `just green` / `just check`,
  which runs the seven-cell `green` matrix against generated GCC and Clang
  compilation databases.
- **JSON-RPC 2.0**: request, notification, response, and error objects;
  `jsonrpc: "2.0"`; string/integer/null ids echoed back; notifications
  omit `id`. Both directions are supported: request construction and
  decoding, response construction and decoding. Standard error codes
  `-32700`, `-32600`, `-32601`, `-32602`, `-32603`, and the
  `-32000..-32099` server range. No batching in V1.
- Transport: the library operates on an already-open, blocking, connected
  POSIX stream-socket file descriptor (for example AF_UNIX or a loopback
  TCP `SOCK_STREAM`); it does not connect, accept, or manage socket
  lifecycle. Framing over the byte stream is newline-delimited JSON
  (NDJSON).
- Four-space indentation.

## `.agent` directory

Keep concept, stories, design, testing (BDD scenarios), and acceptance
documents under `.agent/{concept,stories,design,testing,acceptance}` with
`NNNN-` names. The project must build and run without `.agent`.

## Test-driven development

Every behavior change follows: scenario in `.agent/testing/*.md` → failing
test → minimum code → refactor. Acceptance tests under `.agent/acceptance/*.md`
cover must-exhibit and must-reject behavior. Coverage order: one end-to-end
smoke test first, then unit tests for every pure function and non-trivial
branch, then broader testing.

## `just` and `make`

Use `just` for all actions. Recipes are declared in the root `Justfile`.
`just build` compiles, `just test` runs the suite, `just green` runs the
seven-cell green matrix, `just check` is the green gate, `just lint` runs
shellcheck + clang-format. `just build32`, `just lib32`, `just test32`, and
`just sanitize32` are the best-effort ILP32 recipes; they skip cleanly when
32-bit multilib or the 32-bit sanitizer runtime is absent.

## Git

Keep `main` green. Use short-lived working branches. Do not push without
permission.
