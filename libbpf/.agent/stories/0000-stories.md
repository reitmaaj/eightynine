# 0000 - Stakeholders and value

## Program author

AS someone who writes eBPF programs,
I WANT a runtime that decodes and executes the instruction set defined by
RFC 9669,
SO THAT I can run and inspect bytecode portably without a kernel.

## Embedded / sandbox developer

AS a developer embedding an interpreter,
I WANT a small, strict-C89, warning-clean library exposing decode, validate,
and run,
SO THAT I can audit it fully and link it into a sandboxed host.

## Toolchain consumer

AS a developer of assemblers, disassemblers, or verifiers,
I WANT the instruction encoding and conformance-group membership to be
faithful to RFC 9669,
SO THAT tools agree on byte layout and which instructions are permitted.

## Operator

AS someone running untrusted BPF bytecode,
I WANT a step budget that guarantees termination and bounds-checked memory
access,
SO THAT a misbehaving program cannot hang or read/write out of bounds.

## Audit reviewer

AS someone reviewing for safety,
I WANT the interpreter to reject deprecated instructions, host-specific
immediates, and out-of-range registers,
SO THAT unsupported or unsafe programs fail loudly rather than silently
misbehave.

## CLI user

AS a user on a shell,
I WANT `bpf run <file>`, `bpf dis <file>`, and `bpf groups <file>`,
SO THAT I can execute, disassemble, and inspect the conformance groups of
bytecode without a separate toolchain.
