# 0002-bugfix-acceptance

## Must exhibit

ACCEPT: a 2xx response with malformed (unparsable) JSON returns LLM_EJSON.
ACCEPT: a 2xx response with valid JSON but a missing/wrong-shaped
`choices[0].message.content` returns LLM_EPROTO.
ACCEPT: a streaming request that receives a non-2xx status buffers the error
body so `error.message` is populated from the server's `error.message`.

## Must reject (unacceptable behavior)

REJECT: reporting a syntactically invalid JSON response as a protocol-shape
error (LLM_EPROTO); malformed JSON is a JSON error (LLM_EJSON).
REJECT: a streaming HTTP error losing the server-provided error message and
falling back to a generic status message when the body carried `error.message`.
