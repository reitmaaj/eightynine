# BDD scenarios: JSON-RPC 2.0 protocol core

Covers request decoding and response construction, completing the symmetric
protocol core. Transport scenarios remain in `0001-request-response.md`;
acceptance criteria are in `.agent/acceptance/0001-protocol-core.md`.

## Request decoding

SCENARIO: decode a minimal request
GIVEN `{"jsonrpc":"2.0","method":"ping","id":1}`
WHEN jrpc89_request_decode runs
THEN method is "ping", params is J89_BAD, and the id is integer 1.

SCENARIO: decode a notification
GIVEN a request without an id member
WHEN jrpc89_request_decode runs
THEN id.kind is JRPC89_ID_NONE.

SCENARIO: decode params forms
GIVEN params as an empty array, a non-empty array, an empty object, or a
non-empty object
WHEN jrpc89_request_decode runs
THEN params holds the exact node index and is never J89_BAD.

SCENARIO: decode id forms
GIVEN an id that is an integer, a string, or null
WHEN jrpc89_request_decode runs
THEN the id view preserves kind and value exactly.

SCENARIO: decode method byte strings
GIVEN methods that are empty, Unicode, escaped, embedded-NUL, or long
WHEN jrpc89_request_decode runs
THEN method_len reports the exact byte length.

SCENARIO: ignore unknown members
GIVEN a valid request carrying an additional member
WHEN jrpc89_request_decode runs
THEN it succeeds; JSON-RPC does not forbid extra members.

SCENARIO: duplicate keys are rejected upstream
GIVEN a JSON text with a duplicate protocol member
WHEN libj89 parses it
THEN parsing fails, so jrpc89_request_decode never observes duplicates.

SCENARIO: leave the output unchanged on every decode failure
GIVEN a sentinel-filled jrpc89_request
WHEN jrpc89_request_decode rejects the input
THEN the status is not JRPC89_OK and every output field is unchanged.

## Response construction

SCENARIO: build a result response
GIVEN a result node and an integer, string, or null id
WHEN jrpc89_response_result_new builds the response
THEN it yields jsonrpc "2.0", the result, and the id, and
jrpc89_response_decode accepts the result.

SCENARIO: build an error response
GIVEN a code, a message, and optional data
WHEN jrpc89_response_error_new builds the response
THEN it yields an error object with code and message, omits data when
J89_BAD, and jrpc89_response_decode accepts the error.

SCENARIO: preserve embedded NUL bytes in error messages
GIVEN a message containing an embedded NUL byte
WHEN jrpc89_response_error_new builds the response
THEN the decoded message length and bytes are exact.

SCENARIO: accept every exact error code
GIVEN reserved, application-defined, zero, and negative codes
WHEN jrpc89_response_error_new builds the response
THEN each code is preserved exactly.

SCENARIO: refuse a notification id
GIVEN JRPC89_ID_NONE
WHEN either response builder runs
THEN it refuses; a response must carry an id.

SCENARIO: refuse invalid builder arguments
GIVEN a NULL arena, NULL id, NULL out, a NULL message with nonzero length,
an inexact code, or an invalid result/data node
WHEN a response builder runs
THEN it refuses and `*out` is unchanged.

SCENARIO: propagate a libj89 builder failure
GIVEN an error message containing invalid UTF-8
WHEN jrpc89_response_error_new builds the response
THEN the arena is marked failed and no usable node is returned.

SCENARIO: allocation failure leaves no partial output
GIVEN a deterministic allocation failure at any allocation point
WHEN a response builder runs
THEN it returns JRPC89_ENOMEM and `*out` is unchanged.

## Round trips

SCENARIO: request round trip
GIVEN jrpc89_request_new output
WHEN it is rendered, parsed, and decoded with jrpc89_request_decode
THEN the semantic method, params, and id are identical.

SCENARIO: result round trip
GIVEN jrpc89_response_result_new output
WHEN it is rendered, parsed, and decoded with jrpc89_response_decode
THEN the kind, result node, and id are identical.

SCENARIO: error round trip
GIVEN jrpc89_response_error_new output
WHEN it is rendered, parsed, and decoded with jrpc89_response_decode
THEN code, message bytes, message length, data, and id are identical.
