# 0001-design

## Module boundaries

```text
include/llm89.h     public API; exposes no curl/libj89 types
src/llm89.c         public API + libcurl transport + JSON build/parse glue
src/llm89_sse.c     SSE state machine (no libcurl dependency)
src/llm89_buf.c     growable byte buffer with hard size limits
src/llm89_sse.h     private SSE parser interface
src/llm89_buf.h     private buffer interface
tests/              unit + protocol tests
```

`llm89_sse` and `llm89_buf` are pure and unit-testable without libcurl or
libj89.

## Data flow

Typed request:
`llm_chat`/`llm_chat_stream` validate `llm_chat_request`, build a libj89 object
tree (`j89_object_new`/`j89_array_new`/`j89_string_new`/...), render it with
`j89_render`, then hand the byte string to the libcurl easy interface.

Response:
- non-streaming: libcurl write callback accumulates the body into a bounded
  buffer (`llm89_buf`); on completion libj89 parses it and extracts
  `choices[0].message.content`.
- streaming: the write callback feeds `llm89_sse`; on each complete event
  libj89 parses the payload to extract `choices[0].delta.content`; the library
  emits one `llm_stream_event`.

## Ownership

- Caller owns all input strings for the duration of a public call.
- `llm_client_new` copies configuration; inputs may be freed after success.
- Non-streaming functions transfer allocated `text`/`json` to the caller;
  `llm_response_free` frees them and zeroes the struct.
- Streaming event pointers are valid only until the callback returns.
- Every allocation has one obvious owner; a failed public call leaks nothing.
- `llm_error` is a plain struct; the library never allocates inside it.

## Error mapping

| code              | meaning                                            |
|-------------------|----------------------------------------------------|
| `LLM_OK`          | success                                            |
| `LLM_EINVAL`      | invalid input (role, model, messages, header dup)  |
| `LLM_ENOMEM`      | allocation failure                                 |
| `LLM_ECURL`       | libcurl transport failure                          |
| `LLM_EHTTP`       | HTTP status outside 2xx                            |
| `LLM_EJSON`       | JSON build/parse failure                           |
| `LLM_ESSE`        | malformed SSE                                      |
| `LLM_EPROTO`      | protocol shape error (missing choices/content, no DONE) |
| `LLM_ECANCELLED`  | callback-requested cancellation                    |
| `LLM_EOVERFLOW`   | byte-limit exceeded / size_t->curl_off_t overflow  |

## Key decisions and rationale

- libj89 for JSON (overriding spec's cJSON): the sibling project is strict-C89
  and already vendored in this workspace; it gained a public builder API.
- One libcurl easy handle per client for connection reuse; no multi (V0 is
  blocking). A single client supports no concurrent calls.
- SSE decoder is byte-oriented with its own line handling so arbitrary libcurl
  write boundaries are safe (spec 3.3).
- Limits are build-time constants, kept conservative against unbounded server
  bodies (spec 13.4).
- No redirects, no verification downgrade (spec 15.3, 4.1).
