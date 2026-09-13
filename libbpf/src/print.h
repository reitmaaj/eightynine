#ifndef BPF_PRINT_H
#define BPF_PRINT_H

#include <stddef.h>

#include "decode.h"
#include "validate.h"

/* Human-readable name for a bpf_err value. Never returns NULL. */
const char *bpf_err_name(bpf_err e);

/* Pack a source/destination register pair into the encoding's regs byte. */
bpf_byte bpf_regs_byte(bpf_byte src, bpf_byte dst);

/* Format one instruction as a mnemonic-bearing string into `out`. */
void bpf_dis_one(const bpf_insn *in, char *out);

/* Write the required conformance group names, newline-separated and with
 * base32 first, into `out` (capacity `cap`). */
void bpf_groups_into(bpf_u32 conf, char *out, size_t cap);

#endif
