# 0002-stress-scenarios

SCENARIO: render an over-depth tree fails cleanly
GIVEN a tree nested deeper than J89_MAX_DEPTH
WHEN j89_render is called
THEN it returns a nonzero error without overflowing the stack

SCENARIO: render within the depth limit succeeds
GIVEN a tree nested to exactly J89_MAX_DEPTH
WHEN j89_render is called
THEN it produces valid JSON output without error

These scenarios drive the stress test suite over the vendored JSONTestSuite
corpus in `test/json/` plus generated adversarial inputs.

SCENARIO: corpus negative cases are rejected cleanly
GIVEN every `n_*.json` corpus file
WHEN the j89 CLI parses it
THEN it exits 1 with an error message, never crashes (exit >= 128) and never
exits 0 or 2.

SCENARIO: corpus positive cases within the supported number range are accepted
GIVEN every `y_*.json` corpus file whose numbers fit the supported subset
(no number overflow of `long` or `double`, no duplicate keys)
WHEN the j89 CLI parses it
THEN it exits 0 and emits canonical JSON.

SCENARIO: corpus positive cases outside the numeric range are rejected
GIVEN a `y_*.json` corpus file that is valid RFC 8259 JSON but contains a
number that overflows the supported `long`/`double` range
WHEN the j89 CLI parses it
THEN it exits 1.

SCENARIO: implementation-defined corpus outcomes are pinned
GIVEN the set of `i_*.json` corpus files
WHEN the j89 CLI parses each
THEN the accept/reject outcome matches the recorded pinned baseline exactly.

SCENARIO: deep nesting never overflows the stack
GIVEN JSON with nesting depth above the documented maximum
WHEN the j89 CLI parses it
THEN it exits 1 with a clean error, never terminating via a signal.

SCENARIO: reasonable nesting remains accepted
GIVEN JSON nested within the documented maximum depth (e.g. 500 arrays)
WHEN the j89 CLI parses it
THEN it exits 0.

SCENARIO: adversarial and fuzzed inputs are safe
GIVEN generated inputs (depth ladders, truncated corpus files, seeded random
bytes)
WHEN the j89 CLI parses each
THEN it either accepts (exit 0) or rejects (exit 1), and never crashes or
hangs.
