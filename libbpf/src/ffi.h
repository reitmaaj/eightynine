#ifndef BPF_FFI_H
#define BPF_FFI_H

#include "types.h"
#include "decode.h"

/* Typed effects interface (sandbox Phase 4). An import's signature is an
 * ordered list of argument kinds; each kind consumes guest argument registers.
 * A scalar/capability/fixed-output argument takes one register; an input or
 * output buffer takes two (guest address and length/capacity). The host keeps
 * the signature within the five argument registers r1-r5. */

#define BPF_FFI_MAX 5 /* five guest argument registers */

typedef enum bpf_ffi_kind
{
    BPF_FFI_U32 = 0,
    BPF_FFI_U64,
    BPF_FFI_I32,
    BPF_FFI_I64,
    BPF_FFI_CAP,
    BPF_FFI_IN_BUF,   /* address + length */
    BPF_FFI_OUT_BUF,  /* address + capacity */
    BPF_FFI_OUT_FIXED /* address of a schema-sized record */
} bpf_ffi_kind;

/* Check a signature of `n` argument kinds. Returns BPF_OK and sets *regs to
 * the total guest argument registers required if all kinds are legal and the
 * total is at most BPF_FFI_MAX; otherwise returns BPF_ESIG. `regs` may be
 * null. */
bpf_err bpf_ffi_check(const bpf_ffi_kind *kinds, bpf_u32 n, bpf_u32 *regs);

#endif
