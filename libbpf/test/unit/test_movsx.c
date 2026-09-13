#include <stdio.h>

#include "eval.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

/* MOVSX uses code MOV (0xb) with a nonzero offset; source must be X. */
static bpf_u64 mx64(int width, bpf_u64 src, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode =
        (bpf_byte)((BPF_ALU_MOV << 4) | (BPF_SRC_X << 3) | BPF_CLS_ALU64);
    in.class = BPF_CLS_ALU64;
    in.code = BPF_ALU_MOV;
    in.source = BPF_SRC_X;
    in.mode = 0;
    in.size = 0;
    in.src_reg = 9;
    in.dst_reg = 1;
    in.offset = width;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[9] = src;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[1];
}

static bpf_u32 mx32(int width, bpf_u32 src, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode = (bpf_byte)((BPF_ALU_MOV << 4) | (BPF_SRC_X << 3) | BPF_CLS_ALU);
    in.class = BPF_CLS_ALU;
    in.code = BPF_ALU_MOV;
    in.source = BPF_SRC_X;
    in.mode = 0;
    in.size = 0;
    in.src_reg = 9;
    in.dst_reg = 1;
    in.offset = width;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[9] = (bpf_u64)src;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return (bpf_u32)r.r[1];
}

static void test_movsx64(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = mx64(8, 0x7F, &ok);
    ck(ok && r == 0x7F, "movsx8 positive");
    r = mx64(8, 0xFF, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFFFFUL, "movsx8 -1");
    r = mx64(8, 0x80, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFF80UL, "movsx8 -128");
    r = mx64(16, 0x00008000UL, &ok);
    ck(ok && r == 0xFFFFFFFFFFFF8000UL, "movsx16 -32768");
    r = mx64(16, 0x00007FFFUL, &ok);
    ck(ok && r == 0x7FFF, "movsx16 positive");
    r = mx64(32, 0x80000000UL, &ok);
    ck(ok && r == 0xFFFFFFFF80000000UL, "movsx32 -2^31");
    r = mx64(32, 0x7FFFFFFFUL, &ok);
    ck(ok && r == 0x7FFFFFFFUL, "movsx32 positive");
}

static void test_movsx32(void)
{
    bpf_u32 r;
    bpf_byte ok;

    r = mx32(8, 0x80, &ok);
    ck(ok && r == 0xFFFFFF80u, "movsx32(8) -128");
    r = mx32(8, 0x7F, &ok);
    ck(ok && r == 0x7Fu, "movsx32(8) positive");
    r = mx32(16, 0x8000, &ok);
    ck(ok && r == 0xFFFF8000u, "movsx32(16) -32768");
    r = mx32(16, 0x7FFF, &ok);
    ck(ok && r == 0x7FFFu, "movsx32(16) positive");
}

int main(void)
{
    test_movsx64();
    test_movsx32();
    if (fails)
    {
        (void)fprintf(stderr, "test_movsx: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_movsx\n");
    return 0;
}
