# 0005-sse-crlf-acceptance

## Must exhibit

ACCEPT: feeding an SSE stream whose CRLF line endings are split across feed
calls (the `\r` in one call, the `\n` in the next) yields exactly the same
sequence of events as feeding the entire stream at once.
ACCEPT: multi-line `data:` fields joined by a CRLF split across calls remain a
single event with payload joined by `\n`.

## Must reject (unacceptable behavior)

REJECT: a CRLF boundary split across feed calls causing an event to be
terminated early and delivered as two events instead of one.
