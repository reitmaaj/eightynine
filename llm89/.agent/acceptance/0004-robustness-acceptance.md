# 0004-robustness-acceptance

## Must exhibit

ACCEPT: a non-streaming request that receives a non-JSON (e.g. SSE) body
returns LLM_EJSON without crashing or hanging.
ACCEPT: a chunked transfer-encoding response with only the chunked framing
header (no contradictory Content-Length) is decoded correctly.

## Must reject (unacceptable behavior)

REJECT: a mock/test server emitting both Content-Length and
Transfer-Encoding: chunked on the same response (contradictory framing).
REJECT: a public call returning an ENOMEM result while error.code is unset; the
error code must be consistent with the returned status.
