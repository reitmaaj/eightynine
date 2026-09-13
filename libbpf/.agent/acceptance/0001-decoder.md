# 0001 - Acceptance: decoder

*Acceptance criteria for the decoder milestone. Each item is either a
behaviour the software MUST exhibit or a behaviour it MUST reject.*

## MUST

* `bpf_decode` accepts a byte buffer and length and, given whole 8-byte basic
  instructions, decodes each into a `bpf_insn` with correct `opcode`, `class`
  (low three bits), `code`/`mode`/`size`, `source`, `src_reg`, `dst_reg`
  (split from the regs byte), sign-extended `offset` and `imm`.
* `bpf_decode` decodes a wide instruction (LD with IMM mode, 16 bytes) into a
  single `bpf_insn` with `is_wide` true and both `imm` and `next_imm` set.
* `bpf_decode` decodes multiple instructions sequentially and reports the
  count.
* `bpf_decode` reports the number of decoded instructions.

## MUST NOT

* `bpf_decode` MUST NOT accept a buffer that ends partway through a required
  wide instruction; it MUST report a truncated-instruction error and MUST NOT
  read past the available bytes.
* The decoder MUST NOT read beyond the supplied buffer length.
* Multi-byte fields (`offset`, `imm`, `next_imm`) MUST be read deterministically
  (little-endian) independent of host byte order, so identical buffers decode
  identically everywhere.
