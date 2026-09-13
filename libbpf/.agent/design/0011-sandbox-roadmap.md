# 0011 - Design: sandbox roadmap and next steps

*Status and plan for the libbpf memory/side-effects safety work (RFC 9669
sandbox model). Every change is TDD-gated and kept green under `just test`,
`just check` (c89-baseline + ob89 + bb-lifter), `just lint`, `just ratio`
(>= 3:1 test:source), plus ASan+UBSan and valgrind. Each new source module
roughly triples its test burden because of the ratio gate.*

## Completed and committed (branch `feature/sandbox-phase1`)

| Commit | Phase | Deliverable |
| ------ | ----- | ----------- |
| 4098f31 | 1 | Opaque `bpf_program` loader: strict encodings, slot recording, `r10` contract, unknown-op rejection, slot-resolved branch/call targets, exact-width asserts (`types.h`). |
| 0834c21 | 2a | Opaque `bpf_memory` owned region table + single resolver (`mem.h/c`). |
| e7bedac | 2b | Bytewise little-endian `bpf_mem_load/store` over the resolver. |
| 2123738 | 2c | Opaque `bpf_vm` executor over a loaded program + owned memory; explicit states; slot-resolved control flow (branch-across-wide returns 42). |
| 1c3c0a4 | 2d | Preserved-register frames (r6-r9, r10) across local calls. |
| b1cc272 | 2e | Atomic RMW over owned memory (ADD/OR/AND/XOR/XCHG/CMPXCHG, W/DW). |
| 1902f1c | 3a | Opaque `bpf_caps` capability table (monotonic handles, no reuse, exhaustion, revoke, destroy). |
| ddee053 | 3b | Capability-gated import (helper) dispatch in `bpf_vm`; WAITING/TRAP on gate. |
| 74a28fd | 3c | `bpf_vm_complete`: validated one-shot completion (r0, clear r1-r5, resume); stale/duplicate rejected. |
| 0286a33 | 4a | FFI signature descriptors (`ffi.h/c`): typed argument kinds + register-accounting check. |

## Next steps (in order)

### Phase 4b - FFI argument marshalling
- Extend `ffi` to *lower* a signature against the machine's r1..r5 into a
  validated, host-owned set of marshalled arguments (`bpf_ffi_args`):
  - u32/u64/i32/i64: one register, defined width/extension.
  - capability: one register -> resolve via the machine's `bpf_caps` (live +
    type/rights), yielding a temporary host resource reference.
  - input bytes: address + length -> resolve readable range via
    `bpf_memory`, copy into host staging before dispatch.
  - output bytes: address + capacity -> resolve writable range, bound a host
    staging buffer for copy-back.
  - fixed output value/record: address -> schema-sized writable range.
- Reject overlapping buffer arguments whenever either permits writes; allow
  overlap of input-only buffers (explicit little-endian byte semantics).
- Add unit tests for each kind (valid, boundary, wrong-type cap, insufficient
  rights, cross-region, overlap rules); doc 0021.

### Phase 4c - Uniform handler + synchronous dispatcher
- Define
  `bpf_host_status handler(void *userdata, const bpf_ffi_args *args, bpf_ffi_reply *reply);`
  receiving validated args and host staging, never the mutable machine or raw
  guest pointers.
- Layer a synchronous dispatcher over the request/reply core: snapshot ->
  resolve signature -> validate caps/ranges/quotas -> stage input -> create a
  pending request with a machine-lifetime sequence -> suspend -> dispatch at
  most once -> validate the reply -> copy declared outputs -> `bpf_vm_complete`
  (status) -> resume.
- Validate all outputs before copying any; distinct failure channels (guest
  trap vs. guest-visible status vs. host fault).
- Add a recording handler to prove rejected requests produce zero external
  calls; doc 0022.

### Phase 4d - Stream example + guest wrappers
- `stream.read` example: `(cap<stream,READ>, out_bytes, out_u64 count)` lowered
  to `r1=handle, r2=addr, r3=cap, r4=count`, r0=status; EOF = success count 0.
- Generated-style wrapper helper + e2e-style execution test.

### Phase 5 - Isolation validation
- Adversarial bytecode / malformed replies / capability misuse; stale and
  duplicate dispatch/completion; cleanup during cancellation; recording
  handler proving zero calls on rejection.
- A `just san-fuzz` bytecode fuzzer under ASan+UBSan.

### Phase 6 - Legacy migration (large, separate)
- Remove the public `bpf_machine`/`bpf_step` surface and migrate the ~26
  legacy unit/e2e callers onto the opaque loader + owned machine via shared
  fixtures; update expectations that encoded old defects. This replaces the
  legacy engine, so it is the largest single remaining effort.

## Long-standing constraints
- ISO C89 only; ob89 requires every struct load/call/cast on its own statement
  with witness temporaries; bb-lifter tolerates only skips of struct-heavy
  functions.
- The repo's >= 3:1 test:source ratio means each new source module needs about
  three times its size in tests.
- `_backlog/` at the workspace root is out of scope; all work stays under
  `libbpf`.
