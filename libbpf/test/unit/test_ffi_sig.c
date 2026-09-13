#include <stdio.h>

#include "ffi.h"

static int fails;
static int cases;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static bpf_err chk(const bpf_ffi_kind *k, bpf_u32 n, bpf_u32 *r)
{
    bpf_err e;

    cases = cases + 1;
    e = bpf_ffi_check(k, n, r);
    return e;
}

static void test_empty_sig(void)
{
    bpf_u32 r;
    bpf_err e;

    r = 99;
    e = chk(0, 0, &r);
    ck(e == BPF_OK && r == 0, "empty signature ok, zero registers");
}

static void test_scalar_regs(void)
{
    bpf_ffi_kind k[3];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_U32;
    k[1] = BPF_FFI_U64;
    k[2] = BPF_FFI_CAP;
    r = 0;
    e = chk(k, 3, &r);
    ck(e == BPF_OK && r == 3, "three scalars/caps use three registers");
}

static void test_signed_scalars(void)
{
    bpf_ffi_kind k[2];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_I32;
    k[1] = BPF_FFI_I64;
    r = 0;
    e = chk(k, 2, &r);
    ck(e == BPF_OK && r == 2, "i32/i64 use two registers");
}

static void test_fixed_output_one_reg(void)
{
    bpf_ffi_kind k[3];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_OUT_FIXED;
    k[1] = BPF_FFI_U64;
    k[2] = BPF_FFI_CAP;
    r = 0;
    e = chk(k, 3, &r);
    ck(e == BPF_OK && r == 3, "fixed output takes one register");
}

static void test_buffer_two_regs(void)
{
    bpf_ffi_kind k[2];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_IN_BUF;
    k[1] = BPF_FFI_OUT_BUF;
    r = 0;
    e = chk(k, 2, &r);
    ck(e == BPF_OK && r == 4, "two buffers use four registers");
}

static void test_five_regs_ok(void)
{
    bpf_ffi_kind k[5];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_U32;
    k[1] = BPF_FFI_U64;
    k[2] = BPF_FFI_I64;
    k[3] = BPF_FFI_OUT_FIXED;
    k[4] = BPF_FFI_CAP;
    r = 0;
    e = chk(k, 5, &r);
    ck(e == BPF_OK && r == 5, "five registers accepted");
}

static void test_six_regs_rejected(void)
{
    bpf_ffi_kind k[3];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_IN_BUF;
    k[1] = BPF_FFI_IN_BUF;
    k[2] = BPF_FFI_IN_BUF;
    r = 0;
    e = chk(k, 3, &r);
    ck(e == BPF_ESIG, "three input buffers (six registers) rejected");
}

static void test_over_budget(void)
{
    bpf_ffi_kind k[6];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_U64;
    k[1] = BPF_FFI_U64;
    k[2] = BPF_FFI_U64;
    k[3] = BPF_FFI_U64;
    k[4] = BPF_FFI_U64;
    k[5] = BPF_FFI_U64;
    r = 0;
    e = chk(k, 6, &r);
    ck(e == BPF_ESIG, "six scalar arguments rejected");
}

static void test_unknown_kind_rejected(void)
{
    bpf_ffi_kind k[1];
    bpf_u32 r;
    bpf_err e;

    k[0] = (bpf_ffi_kind)200;
    e = chk(k, 1, &r);
    ck(e == BPF_ESIG, "unknown kind rejected");
}

static void test_exactly_five_ok(void)
{
    bpf_ffi_kind k[5];
    bpf_u32 r;
    bpf_err e;

    k[0] = BPF_FFI_U32;
    k[1] = BPF_FFI_U64;
    k[2] = BPF_FFI_U32;
    k[3] = BPF_FFI_OUT_FIXED;
    k[4] = BPF_FFI_U64;
    r = 0;
    e = chk(k, 5, &r);
    ck(e == BPF_OK && r == 5, "signature exactly five registers ok");
}

static void test_regs_null_output(void)
{
    bpf_ffi_kind k[2];
    bpf_err e;

    k[0] = BPF_FFI_U64;
    k[1] = BPF_FFI_CAP;
    e = chk(k, 2, 0);
    ck(e == BPF_OK, "null regs output accepted");
}

int main(void)
{
    test_empty_sig();
    test_scalar_regs();
    test_signed_scalars();
    test_fixed_output_one_reg();
    test_buffer_two_regs();
    test_five_regs_ok();
    test_six_regs_rejected();
    test_over_budget();
    test_unknown_kind_rejected();
    test_exactly_five_ok();
    test_regs_null_output();
    if (fails)
    {
        (void)fprintf(stderr, "test_ffi_sig: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_ffi_sig (%d cases)\n", cases);
    return 0;
}
