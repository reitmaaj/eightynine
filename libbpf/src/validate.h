#ifndef BPF_VALIDATE_H
#define BPF_VALIDATE_H

#include "decode.h"

/* Conformance group bit flags (RFC 9669 section 7.1). */
#define BPF_CONF_BASE32 0x00000001u
#define BPF_CONF_BASE64 0x00000002u
#define BPF_CONF_DIVMUL32 0x00000004u
#define BPF_CONF_DIVMUL64 0x00000008u
#define BPF_CONF_ATOMIC32 0x00000010u
#define BPF_CONF_ATOMIC64 0x00000020u
#define BPF_CONF_PACKET 0x00000040u

/* Valid byte-swap widths, RFC 9669 section 4.2. */
#define BPF_END_16 16
#define BPF_END_32 32
#define BPF_END_64 64

/* Structurally validate a decoded program and compute the union of the
 * conformance groups it requires. Returns BPF_OK and sets `*conf` on
 * success; otherwise returns the first error found. */
bpf_err bpf_validate(const bpf_insn *ins, bpf_u32 n, bpf_u32 *conf);

/* Compute just the conformance group bits for a single instruction. */
bpf_u32 bpf_insn_conf(const bpf_insn *in);

#endif
