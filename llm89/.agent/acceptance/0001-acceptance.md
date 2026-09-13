# 0001-acceptance

## Must exhibit

ACCEPT: `llm_global_init`/`llm_global_cleanup` manage libcurl global state
(unless `LLM89_EXTERNAL_CURL_GLOBALS` is defined).
ACCEPT: `llm_client_new` copies all configuration and returns a reusable client
owning one libcurl easy handle.
ACCEPT: a non-stream `llm_chat` performs one POST to the configured endpoint and
stores the full response JSON in `response->json` and
`choices[0].message.content` in `response->text`.
ACCEPT: a streamed `llm_chat_stream` delivers one `LLM_EVENT_DATA` callback per
complete SSE `data:` event, with `text` set to `choices[0].delta.content` when
present, and exactly one `LLM_EVENT_DONE` for the terminal `[DONE]`.
ACCEPT: `llm_json`/`llm_json_stream` send the caller-provided JSON body unchanged
and expose raw response/event JSON without interpreting the schema.
ACCEPT: `api_key` produces `Authorization: Bearer <key>`; a NULL key produces no
authorization header.
ACCEPT: `Content-Type: application/json` is always sent; `Accept:
text/event-stream` for streaming and `Accept: application/json` otherwise.
ACCEPT: the caller may append custom headers, and duplicate
security-sensitive headers (`Authorization`, `Content-Type`) are rejected.
ACCEPT: connect and total timeouts apply; `cancel` may abort a request,
returning `LLM_ECANCELLED`.
ACCEPT: the SSE decoder accepts LF, CRLF, and CR, strips one optional space
after `data:`, concatenates multi-line `data` with `\n`, and ignores comments
and `event`/`id`/`retry`/unknown fields.
ACCEPT: build-time limits `LLM89_MAX_RESPONSE_BYTES`,
`LLM89_MAX_SSE_EVENT_BYTES`, `LLM89_MAX_ERROR_BODY_BYTES` abort with
`LLM_EOVERFLOW`.
ACCEPT: `llm_response_free` releases text/json and zeroes the structure;
`llm_error` allocates nothing and accepts NULL.
ACCEPT: the full test suite passes under a strict-C89 build and under
AddressSanitizer.

## Must reject (unacceptable behavior)

REJECT: invalid roles, missing/empty model, zero messages, NULL message content,
negative `max_tokens`, or non-finite temperature returning success or touching
the network.
REJECT: any typed or raw request issuing network activity after a local
validation failure.
REJECT: a caller-supplied duplicate `Authorization` or `Content-Type` header
being sent.
REJECT: a raw JSON request body that is not one top-level JSON object being
sent.
REJECT: an HTTP status outside `[200,299]` returning anything but `LLM_EHTTP`
(regardless of body content).
REJECT: a non-streaming response missing `choices[0].message.content` (no
`choices`, empty `choices`, no `message`, or non-string content) returning
success; it must be `LLM_EPROTO`.
REJECT: a streaming response whose body ends before a `[DONE]` event; it must
return `LLM_EPROTO`.
REJECT: `[DONE]` being parsed as JSON.
REJECT: a nonzero `stream` callback return or a nonzero `cancel` callback return
returning anything but `LLM_ECANCELLED`.
REJECT: redirects being followed (no credential forwarding to another origin).
REJECT: TLS peer/host certificate verification being disabled through any API.
REJECT: one client executing two calls concurrently; reuse of a client in the
middle of another call (undefined behavior / debug assert).
REJECT: a `size_t` request length that cannot fit a `curl_off_t` being passed to
libcurl without `LLM_EOVERFLOW`.
REJECT: any public header exposing a libcurl or libj89 type.
REJECT: unbounded memory growth: exceeding the configured byte limits must abort
the transfer, not continue buffering.
