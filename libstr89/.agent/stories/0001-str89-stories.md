# 0001 — libstr89 stakeholders

## JSON parser and builder authors (`libj89`)

AS a JSON parser/builder author
I WANT strings and object keys stored as validated UTF-8 with explicit
lengths, so that
SO THAT embedded NUL bytes survive, malformed UTF-8 is rejected before
storage, and I never need a parallel pointer/length/UTF-8 mechanism.

Value gained: one string type; validation at the boundary; exact byte
preservation; no `strlen()` truncation of keys; atomic mutation failure.

## Protocol and storage authors (`libjrpc89`, future `libledger89`)

AS a protocol/storage author
I WANT an owning string type with an explicit allocator and transactional
mutators, so that
SO THAT I can build and edit wire/storage text without leaking on failure and
without inventing a fourth string representation.

Value gained: allocator control, failure atomicity, byte-exact comparison and
search, embedded NUL support.

## Application authors

AS an application author
I WANT a small, boring string library that never transforms my text, so that
SO THAT user data round-trips byte-for-byte and locale never silently changes
equality or ordering.

Value gained: predictable semantics; no hidden normalization, case folding, or
collation.
