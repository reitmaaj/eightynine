# 0001-scenarios

## SSE decoder

SCENARIO: deliver a single data event
GIVEN bytes `data:hello\n\n`
WHEN fed to the SSE parser one byte at a time
THEN one event is delivered with payload `hello`.

SCENARIO: byte-partition independence
GIVEN identical SSE bytes fed (a) one byte at a time, (b) all at once, and (c)
at every possible split point
WHEN the parser consumes them
THEN the sequence of delivered payloads is identical.

SCENARIO: accept LF, CRLF, and CR
GIVEN `data:a\n\n`, `data:a\r\n\r\n`, and `data:a\r\r`
WHEN parsed
THEN each yields the payload `a`.

SCENARIO: split line ending across calls
GIVEN `data:a\r` fed in one call and `\n\n` fed in the next
WHEN parsed
THEN the CRLF boundary is handled and one event `a` is delivered.

SCENARIO: multi-line data concatenation
GIVEN `data:a\ndata:b\n\n`
WHEN parsed
THEN one event is delivered with payload `a\nb`.

SCENARIO: strip one optional space after colon
GIVEN `data: x\n\n` and `data:x\n\n`
WHEN parsed
THEN both yield payload `x`.

SCENARIO: ignore comments and other fields
GIVEN lines starting with `:`, and `event:`/`id:`/`retry:`/unknown fields
alongside a `data:` line
WHEN parsed
THEN only the `data` payload is delivered.

SCENARIO: empty event delivers nothing
GIVEN a blank line with no preceding `data:` line
WHEN parsed
THEN no event callback fires.

SCENARIO: terminal DONE token
GIVEN a data payload equal to `[DONE]`
WHEN parsed
THEN the caller is signalled DONE and the payload is not parsed as JSON.

SCENARIO: event over the size limit
GIVEN a single data payload longer than `LLM89_MAX_SSE_EVENT_BYTES`
WHEN parsed
THEN the parser reports overflow and aborts.

SCENARIO: split UTF-8 code point across calls
GIVEN a multi-byte UTF-8 payload split across parser calls
WHEN parsed
THEN the assembled payload preserves the original bytes.

## JSON request construction

SCENARIO: roles map exactly
GIVEN roles SYSTEM/USER/ASSISTANT
WHEN a typed request is built
THEN the JSON emits `"system"`/`"user"`/`"assistant"` respectively.

SCENARIO: required fields
GIVEN a model and one or more messages
WHEN a typed request is built
THEN JSON has `"model"`, a non-empty `"messages"` array, and `"stream"`.

SCENARIO: optional parameters
GIVEN `set_temperature`/`set_max_tokens`
WHEN present
THEN `"temperature"`/`"max_tokens"` are emitted; when absent they are omitted.

SCENARIO: escaping
GIVEN message content with quotes, backslashes, control bytes, and UTF-8
WHEN built
THEN the JSON string is correctly escaped and round-trips.

SCENARIO: reject invalid inputs
GIVEN an invalid role, missing/empty model, zero messages, NULL content,
negative max_tokens, or non-finite temperature
WHEN a typed request is validated
THEN it is rejected before any network activity.

SCENARIO: raw JSON validation
GIVEN a raw request body
WHEN it is not one top-level JSON object
THEN it is rejected before any network activity.

## Response parsing

SCENARIO: non-streaming extraction
GIVEN JSON with `choices[0].message.content` as a string
WHEN parsed
THEN `response->text` holds that string and `response->json` holds the body.

SCENARIO: protocol errors
GIVEN JSON missing `choices`, with empty `choices`, missing `message`, or
non-string `content`
WHEN parsed
THEN `LLM_EPROTO` is returned.

SCENARIO: streaming delta extraction
GIVEN an SSE event whose JSON has `choices[0].delta.content` as a string
WHEN delivered
THEN `event->text`/`text_len` carry it; otherwise they are NULL/0.

## Transport

SCENARIO: HTTP error handling
GIVEN a status outside `[200,299]`
WHEN the request completes
THEN `LLM_EHTTP` is returned with `http_status` set and, when present,
`error.message` from `error.message` in the body.

SCENARIO: stream callback cancellation
GIVEN a `stream` callback returning nonzero
WHEN the transfer is active
THEN the transfer aborts and `LLM_ECANCELLED` is returned.

SCENARIO: cancel callback polling
GIVEN a `cancel` callback that returns nonzero
WHEN libcurl invokes the progress callback
THEN the transfer aborts and `LLM_ECANCELLED` is returned.

SCENARIO: total timeout
GIVEN a `timeout_ms` that elapses before completion
WHEN the transfer runs
THEN `LLM_ECURL` is returned with the transport error recorded.

SCENARIO: response size limit
GIVEN a response body longer than `LLM89_MAX_RESPONSE_BYTES`
WHEN buffered
THEN `LLM_EOVERFLOW` is returned and the transfer aborts.

SCENARIO: duplicate sensitive header rejection
GIVEN a caller-supplied `Authorization` or `Content-Type` header
WHEN the client is constructed
THEN it is rejected with `LLM_EINVAL`.
