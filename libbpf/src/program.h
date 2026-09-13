#ifndef BPF_PROGRAM_H
#define BPF_PROGRAM_H

#include "decode.h"
#include "validate.h"

/* Opaque, immutable, validated program. Loading is the only route into
 * execution (RFC 9669 sandbox model). The loader decodes a strict raw
 * encoding, rejects unknown/reserved operations and forms, enforces the
 * frame-pointer contract, resolves every branch and local call to an
 * instruction index measured in 64-bit slots, and checks the requested
 * execution profile. */
typedef struct bpf_program bpf_program;

/* Execution profile requested by the host. `allowed` is the union of
 * permitted BPF_CONF_* conformance-group bits; a program requiring any group
 * outside `allowed` is rejected. `allowed == 0` permits every supported group.
 * `max_insn` bounds the decoded instruction count; `max_insn == 0` imposes no
 * additional cap. */
typedef struct bpf_profile
{
    bpf_u32 allowed;
    bpf_u32 max_insn;
} bpf_profile;

/* Read-only per-instruction metadata reported by the loader. */
typedef struct bpf_prog_insn
{
    bpf_insn in;            /* decoded semantic fields */
    bpf_u32 slot;           /* starting 64-bit slot index of this instruction */
    bpf_byte is_wide;       /* occupies two 64-bit slots */
    bpf_byte is_call_local; /* resolved local function call */
    bpf_byte has_target;    /* branch/local-call has a resolved target */
    bpf_u32 target;         /* resolved taken-target instruction index */
} bpf_prog_insn;

/* Decode, strictly validate, and resolve `len` bytes of raw eBPF bytecode
 * into a new immutable program. On success sets *out and returns BPF_OK; the
 * caller owns the program and must destroy it. On failure leaves *out
 * unchanged and returns a specific error code. */
bpf_err bpf_program_load(const bpf_byte *raw, bpf_u32 len,
                         const bpf_profile *prof, bpf_program **out);

/* Release all resources owned by a program. */
void bpf_program_destroy(bpf_program *prog);

/* Number of decoded instructions in a program. */
bpf_u32 bpf_program_count(const bpf_program *prog);

/* Union of the conformance-group bits required by the program. */
bpf_u32 bpf_program_conformance(const bpf_program *prog);

/* Copy the resolved metadata for instruction `idx` into *out. Returns BPF_OK,
 * or BPF_EREG if `idx` is out of range. */
bpf_err bpf_program_get(const bpf_program *prog, bpf_u32 idx,
                        bpf_prog_insn *out);

#endif
