#include <stdio.h>

#include "decode.h"

static int check(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

static void put_le16(bpf_byte *p, bpf_u32 v)
{
    p[0] = (bpf_byte)(v & 0xffu);
    p[1] = (bpf_byte)((v >> 8) & 0xffu);
}

static void put_le32(bpf_byte *p, bpf_u32 v)
{
    p[0] = (bpf_byte)(v & 0xffu);
    p[1] = (bpf_byte)((v >> 8) & 0xffu);
    p[2] = (bpf_byte)((v >> 16) & 0xffu);
    p[3] = (bpf_byte)((v >> 24) & 0xffu);
}

static int test_basic(void)
{
    bpf_byte buf[8];
    bpf_insn ins[2];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0xb7; /* code MOV (0xb<<4), s=0, class ALU64 (0x7) */
    buf[1] = 0x01; /* src=0, dst=1 */
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0x11223344u);
    e = bpf_decode(buf, 8, ins, 2, &n);
    fail |= check(e == BPF_OK, "basic: ok");
    fail |= check(n == 1, "basic: one insn");
    fail |= check(ins[0].class == BPF_CLS_ALU64, "basic: class ALU64");
    fail |= check(ins[0].code == BPF_ALU_MOV, "basic: code MOV");
    fail |= check(ins[0].source == BPF_SRC_K, "basic: source K");
    fail |= check(ins[0].dst_reg == 1, "basic: dst 1");
    fail |= check(ins[0].src_reg == 0, "basic: src 0");
    fail |= check(ins[0].offset == 0, "basic: offset 0");
    fail |= check(ins[0].imm == (int)0x11223344u, "basic: imm");
    fail |= check(ins[0].is_wide == 0, "basic: not wide");
    return fail;
}

static int test_offset_signextend(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0x05; /* JA (code 0), s=0, class JMP (0x5) */
    buf[1] = 0x00;
    put_le16(buf + 2, 0xFFFFu); /* -1 */
    put_le32(buf + 4, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_OK, "offsig: ok");
    fail |= check(ins[0].offset == -1, "offsig: offset -1");
    return fail;
}

static int test_imm_signextend(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0xb7;
    buf[1] = 0x01;
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0xFFFFFFFFu);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_OK, "immsig: ok");
    fail |= check(ins[0].imm == -1, "immsig: imm -1");
    return fail;
}

static int test_wide(void)
{
    bpf_byte buf[16];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0x18; /* mode IMM (0), size DW (3), class LD (0) */
    buf[1] = 0x01; /* src=0 (subtype), dst=1 */
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0x11111111u);
    put_le32(buf + 8, 0);            /* reserved */
    put_le32(buf + 12, 0x22222222u); /* next_imm */
    e = bpf_decode(buf, 16, ins, 1, &n);
    fail |= check(e == BPF_OK, "wide: ok");
    fail |= check(n == 1, "wide: one insn");
    fail |= check(ins[0].is_wide == 1, "wide: is_wide");
    fail |= check(ins[0].imm == (int)0x11111111u, "wide: imm");
    fail |= check(ins[0].next_imm == 0x22222222u, "wide: next_imm");
    fail |= check(ins[0].src_reg == 0, "wide: subtype 0");
    fail |= check(ins[0].dst_reg == 1, "wide: dst 1");
    return fail;
}

static int test_truncated_wide(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0x18; /* wide LD IMM */
    buf[1] = 0x01;
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_ETRUNC, "trunc: ETRUNC");
    return fail;
}

static int test_multiple(void)
{
    bpf_byte buf[16];
    bpf_insn ins[2];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0xb7; /* MOV64 K r1, imm=5 */
    buf[1] = 0x01;
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 5);
    buf[8] = 0xb7; /* MOV64 K r2, imm=7 */
    buf[9] = 0x02;
    put_le16(buf + 10, 0);
    put_le32(buf + 12, 7);
    e = bpf_decode(buf, 16, ins, 2, &n);
    fail |= check(e == BPF_OK, "multi: ok");
    fail |= check(n == 2, "multi: two insns");
    fail |= check(ins[0].dst_reg == 1 && ins[0].imm == 5, "multi: first");
    fail |= check(ins[1].dst_reg == 2 && ins[1].imm == 7, "multi: second");
    return fail;
}

static int test_regs_split(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0xbf; /* MOV X ALU64 */
    buf[1] = 0x12; /* src=1, dst=2 */
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_OK, "regs: ok");
    fail |= check(ins[0].src_reg == 1, "regs: src 1");
    fail |= check(ins[0].dst_reg == 2, "regs: dst 2");
    return fail;
}

static int test_classes(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 8; ++i)
    {
        /* MEM mode, DW size, class i: a basic (non-wide) instruction. */
        buf[0] =
            (bpf_byte)((BPF_MODE_MEM << 5) | (BPF_SIZE_DW << 3) | (bpf_byte)i);
        buf[1] = 0;
        put_le16(buf + 2, 0);
        put_le32(buf + 4, 0);
        e = bpf_decode(buf, 8, ins, 1, &n);
        fail |= check(e == BPF_OK && ins[0].class == (bpf_byte)i, "class loop");
    }
    return fail;
}

static int test_modesize(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int fail;

    fail = 0;
    buf[0] = 0x0F; /* ADD X ALU64: code 0, s 1, class 7 */
    buf[1] = 0x10; /* src=1, dst=0 */
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_OK, "ms: alu ok");
    fail |= check(ins[0].code == BPF_ALU_ADD && ins[0].source == BPF_SRC_X,
                  "ms: ADD X");
    fail |= check(ins[0].class == BPF_CLS_ALU64, "ms: class ALU64");

    buf[0] = 0x73; /* mode MEM(3)<<5, size B(2)<<3, class STX(3) = 0x73 */
    buf[1] = 0x00;
    put_le16(buf + 2, 0);
    put_le32(buf + 4, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    fail |= check(e == BPF_OK, "ms: stx ok");
    fail |= check(ins[0].mode == BPF_MODE_MEM && ins[0].size == BPF_SIZE_B,
                  "ms: mode MEM size B");
    fail |= check(ins[0].class == BPF_CLS_STX, "ms: class STX");
    return fail;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_basic();
    fail |= test_offset_signextend();
    fail |= test_imm_signextend();
    fail |= test_wide();
    fail |= test_truncated_wide();
    fail |= test_multiple();
    fail |= test_regs_split();
    fail |= test_classes();
    fail |= test_modesize();
    if (fail)
    {
        (void)fprintf(stderr, "test_decode: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_decode\n");
    return 0;
}
