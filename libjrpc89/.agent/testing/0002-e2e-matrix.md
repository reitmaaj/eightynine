# BDD scenarios: e2e RPC exchange matrix

## Passing exchanges

SCENARIO: return a primitive result
GIVEN the server replies with a valid primitive result and a matching id
WHEN the CLI runs one exchange
THEN it prints the rendered result and exits 0.

SCENARIO: return an array result
GIVEN the server replies with a valid array result and a matching id
WHEN the CLI runs one exchange
THEN it prints the rendered result and exits 0.

SCENARIO: return an object result
GIVEN the server replies with a valid object result and a matching id
WHEN the CLI runs one exchange
THEN it prints the rendered result and exits 0.

SCENARIO: return a result of any valid params shape
GIVEN a params argument that is valid JSON of any shape
WHEN the CLI builds and sends the request
THEN it succeeds and echoes the corresponding result.

SCENARIO: return a result up to the buffer limit
GIVEN a result whose payload is up to cap-1 bytes
WHEN the CLI reads the frame
THEN it prints the full result and exits 0.

SCENARIO: send a request without params
GIVEN no params argument is provided
WHEN the CLI builds the request
THEN the request carries no params member and the exchange succeeds.

## Failing exchanges

SCENARIO: handle a server error response
GIVEN the server replies with a valid error object and a matching id
WHEN the CLI runs one exchange
THEN it prints a structured error line and does not emit a result.

SCENARIO: handle a reserved error code
GIVEN a server error whose code lies in -32000..-32099 or is a standard code
WHEN the CLI classifies it
THEN the error line marks it as reserved.

SCENARIO: handle an application error code
GIVEN a server error whose code lies outside the reserved ranges
WHEN the CLI classifies it
THEN the error line marks it as application-defined.

SCENARIO: reject a malformed response
GIVEN a response that is not an object, has a wrong jsonrpc version, carries
both result and error, carries neither, or lacks an id
WHEN the CLI validates it
THEN it exits nonzero without printing a result.

SCENARIO: reject a non-object error member
GIVEN a response whose error member is not an object
WHEN the CLI validates it
THEN it exits nonzero.

SCENARIO: reject a truncated frame
GIVEN the peer closes after partial bytes and no newline
WHEN the CLI reads the frame
THEN it exits nonzero.

SCENARIO: reject an oversized frame
GIVEN a frame whose payload exceeds the buffer capacity
WHEN the CLI reads the frame
THEN it exits nonzero.

SCENARIO: reject a mismatched response id
GIVEN a response whose id differs from the request id
WHEN the CLI validates it
THEN it exits nonzero instead of printing a result.

SCENARIO: reject an empty method
GIVEN an empty method name
WHEN the CLI builds the request
THEN it exits nonzero.

SCENARIO: reject invalid params JSON
GIVEN a params argument that is not valid JSON
WHEN the CLI builds the request
THEN it reports an error and exits nonzero instead of silently omitting
params.
