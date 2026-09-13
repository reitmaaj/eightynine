#include "ffi.h"

static bpf_u32 kind_regs(bpf_ffi_kind k)
{
    bpf_u32 r;

    r = 1;
    if (k == BPF_FFI_IN_BUF)
    {
        r = 2;
    }
    if (k == BPF_FFI_OUT_BUF)
    {
        r = 2;
    }
    return r;
}

static bpf_byte kind_ok(bpf_ffi_kind k)
{
    bpf_byte r;

    r = 1;
    if (k > BPF_FFI_OUT_FIXED)
    {
        r = 0;
    }
    return r;
}

bpf_err bpf_ffi_check(const bpf_ffi_kind *kinds, bpf_u32 n, bpf_u32 *regs)
{
    bpf_u32 i;
    bpf_u32 total;
    bpf_byte ok;

    ok = 1;
    total = 0;
    i = 0;
    while (i < n)
    {
        bpf_ffi_kind k;
        bpf_byte ko;
        bpf_u32 kr;

        k = kinds[i];
        ko = kind_ok(k);
        if (!ko)
        {
            ok = 0;
        }
        kr = kind_regs(k);
        total = total + kr;
        i = i + 1;
    }
    if (total > BPF_FFI_MAX)
    {
        ok = 0;
    }
    if (!ok)
    {
        return BPF_ESIG;
    }
    if (regs != 0)
    {
        *regs = total;
    }
    return BPF_OK;
}
