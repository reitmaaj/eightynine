# 0020 - Acceptance: FFI signature descriptors

*Acceptance criteria for the typed effects interface descriptors. Each item is
a behaviour the software MUST exhibit or MUST reject.*

## MUST

* A signature MUST be an ordered list of argument kinds drawn from the defined
  vocabulary: u32, u64, i32, i64, capability, input bytes, output bytes, and a
  fixed output value/record.
* Register accounting MUST count a scalar/capability/fixed-output argument as
  one register and an input/output buffer argument as two (address and
  length/capacity).
* A signature whose total register use is at most five MUST be accepted.

## MUST NOT

* A signature whose total register use exceeds five MUST NOT be accepted.
* A signature containing a kind outside the vocabulary MUST NOT be accepted.
* Record layouts MUST NOT rely on native C struct layout (kept explicit and
  out of this descriptor's scope).
