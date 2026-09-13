# Stakeholders

## Storage and protocol consumers

AS a storage or protocol component (for example `libledger89`, `libraft89`,
or `libj89`)

I WANT fixed checksum algorithms with caller-owned streaming state and no
hidden allocation, I/O, or dispatch

SO THAT I can compute record, frame, and message integrity without
inheriting policy, infrastructure, or an error domain that cannot occur.

## Format owners

AS the owner of an on-disk or on-wire format

I WANT numeric checksum values only, with serialization left to me

SO THAT the same algorithm can be framed differently across Ethernet, SCTP,
NVMe, filesystems, and test vectors without the checksum library guessing.

## Test authors and auditors

AS a maintainer of this library

I WANT frozen algorithm parameters, published check values, a test-only
bit-at-a-time reference model, and independent table-entry regeneration

SO THAT a corrupted table, a streaming boundary bug, or an algorithm
substitution fails a fast deterministic test rather than a later integration.

## Callers branching a stream

AS a caller that must compute several checksums over overlapping input

I WANT contexts that copy by ordinary C assignment and a `final` that does
not mutate

SO THAT I can fork a streaming computation with `b = a;` and inspect
intermediate values without destroying the state.
