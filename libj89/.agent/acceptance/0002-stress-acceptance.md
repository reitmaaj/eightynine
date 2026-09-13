# 0002-stress-acceptance

## Must exhibit

ACCEPT: every `n_*.json` corpus file is rejected with exit code 1, an error
message, and no crash, no hang, and no exit code 0 or 2.

ACCEPT: every `y_*.json` corpus file restricted to the libj89 subset (integer
numbers only, no overflow) is accepted and rendered as canonical JSON.

ACCEPT: every `y_*.json` corpus file that is valid RFC 8259 but uses a
fraction, exponent, or overflowing integer is rejected with exit code 1.

ACCEPT: the accept/reject outcome for each `i_*.json` corpus file matches the
pinned baseline (recorded in the corpus harness).

ACCEPT: input nested beyond the documented maximum depth is rejected with a
clean error rather than a process signal.

ACCEPT: input nested within the documented maximum depth is accepted.

ACCEPT: `j89_render` of a tree nested beyond the documented maximum depth
fails cleanly with a nonzero return rather than overflowing the stack.

ACCEPT: every generated adversarial input (depth ladder, truncated corpus,
seeded random bytes) is handled by accepting or rejecting; none crash or hang.

## Must reject (unacceptable behavior)

REJECT: any process termination by signal (segmentation fault, stack
overflow) on any input, valid or invalid, or on rendering any tree.

REJECT: a hang or unbounded run time on any generated adversarial input.

REJECT: a valid RFC 8259 value that libj89 claims to support being rejected
(i.e. a false negative within the subset).

REJECT: an `i_*.json` outcome that drifts from the pinned baseline without an
accompanying documented change of intent.
