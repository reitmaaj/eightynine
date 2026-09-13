# 0005-sse-crlf-scenarios

SCENARIO: CRLF split across feed calls must not split an event
GIVEN a single SSE event built from two `data:` lines (`data:a\r\ndata:b\r\n\r\n`)
WHEN the CRLF between the two lines is split so the `\r` ends one feed and the
`\n` begins the next
THEN exactly one event with payload `a\nb` is delivered
AND the result is identical to feeding the whole byte string at once.

SCENARIO: lone CR line ending at a feed boundary
GIVEN a `\r` as the final byte of one feed call
WHEN the next feed begins with a non-`\n` byte
THEN that byte starts a fresh line and is not skipped.
