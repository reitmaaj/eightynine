# BDD scenarios: JSON-RPC 2.0 client

## Request building

SCENARIO: build a request with an integer id
GIVEN a method name and an integer id
WHEN jrpc89_request_new builds the request object
THEN it yields jsonrpc "2.0", the method, the params, and the id echo.

SCENARIO: build a request with a string id
GIVEN a method name and a string id
WHEN jrpc89_request_new builds the request object
THEN the id member is the exact string bytes.

SCENARIO: build a request with a null id
GIVEN a method name and a null id
WHEN jrpc89_request_new builds the request object
THEN the id member is the JSON null literal.

SCENARIO: build a notification
GIVEN a method name and no id
WHEN jrpc89_request_new builds the request object
THEN the object has no id member.

SCENARIO: build a request with params
GIVEN a method name, an id, and a params node
WHEN jrpc89_request_new builds the request object
THEN the params member is present and equals the given node.

SCENARIO: reject an empty method
GIVEN an empty or NUL method name
WHEN jrpc89_request_new builds the request object
THEN it reports an error and produces no usable object.

SCENARIO: reject a NULL method pointer
GIVEN a NULL method name
WHEN jrpc89_request_new builds the request object
THEN it refuses without dereferencing the pointer and produces no usable object.

SCENARIO: propagate a libj89 builder failure
GIVEN a method containing invalid UTF-8
WHEN jrpc89_request_new builds the request object
THEN the arena is marked failed and no usable object is returned, rather than an
apparently valid node from a failed construction.

SCENARIO: refuse a pre-failed arena
GIVEN an arena already marked failed by an earlier builder failure
WHEN jrpc89_request_new builds the request object
THEN it refuses immediately and produces no usable object.

## Response parsing

SCENARIO: parse a successful response
GIVEN a response object with result and a matching id
WHEN jrpc89_response_validate runs
THEN it accepts it, reports not-error, and yields the result node.

SCENARIO: parse an error response
GIVEN a response object with error, code, message, and a matching id
WHEN jrpc89_response_validate runs
THEN it accepts it, reports error, and yields code/message/data.

SCENARIO: reject a response with a non-"2.0" jsonrpc version
GIVEN a response object whose jsonrpc member is not "2.0"
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a response with both result and error
GIVEN a response object carrying both result and error members
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a response with neither result nor error
GIVEN a response object with neither result nor error members
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a response with a non-object error
GIVEN a response whose error member is not an object
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject an error object without a code
GIVEN a response whose error object has no code member
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject an error object without a message
GIVEN a response whose error object has no message member
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a non-integer error code
GIVEN a response whose error code is not an integer
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a non-string error message
GIVEN a response whose error message is not a string
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject a response without an id
GIVEN a response object with result but no id member
WHEN jrpc89_response_validate runs
THEN it rejects it.

SCENARIO: reject an illegal response id kind
GIVEN a response whose id is a boolean, float, array, or object
WHEN jrpc89_response_validate runs
THEN it rejects it rather than silently treating the id as null.

SCENARIO: reject a mismatched response id
GIVEN a response whose id differs from the request id
WHEN jrpc89_id_matches compares them
THEN it reports no match.

SCENARIO: CLI rejects a mismatched response id
GIVEN the CLI sends a request with id 1
WHEN the response carries a different id
THEN the CLI reports a mismatch and exits nonzero instead of printing a result.

## Error codes

SCENARIO: classify a reserved server code
GIVEN an error code in -32768..-32000
WHEN jrpc89_error_is_reserved classifies it
THEN it reports reserved.

SCENARIO: classify an application code
GIVEN an error code outside -32768..-32000
WHEN jrpc89_error_is_reserved classifies it
THEN it reports application-defined.

SCENARIO: preserve an error code outside the C int range
GIVEN an error code within libj89's exact integer range but outside INT_MIN..INT_MAX
WHEN jrpc89_error_code extracts it
THEN the exact j89_int value is returned rather than a narrowed int.

## Framing

SCENARIO: write then read a frame
GIVEN a JSON string
WHEN jrpc89_write_frame writes it and jrpc89_read_frame reads the socket
THEN the read bytes equal the written JSON without the trailing newline.

SCENARIO: read a frame that exactly fills the buffer
GIVEN a payload of cap-1 bytes followed by a newline
WHEN jrpc89_read_frame reads it into a cap-byte buffer
THEN it succeeds and reports the full cap-1 byte payload.

SCENARIO: report a frame too long for the buffer
GIVEN a payload of cap or more bytes with no newline before the buffer fills
WHEN jrpc89_read_frame reads it into a cap-byte buffer
THEN it reports the frame as too long.

SCENARIO: report a truncated frame
GIVEN the peer closes after sending partial bytes and no newline
WHEN jrpc89_read_frame reads the socket
THEN it reports the frame as truncated rather than as too long or success.
