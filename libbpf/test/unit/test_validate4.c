#include <stdio.h>

#include "validate.h"

static int fails;
static int count;

static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;
    bpf_byte cls;

    cls = (bpf_byte)(op & 0x07u);
    i.opcode = op;
    i.class = cls;
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    if (cls == BPF_CLS_ALU || cls == BPF_CLS_ALU64 || cls == BPF_CLS_JMP ||
        cls == BPF_CLS_JMP32)
    {
        i.mode = 0;
        i.size = 0;
    }
    else
    {
        i.mode = (bpf_byte)(op >> 5);
        i.size = (bpf_byte)((op >> 3) & 0x03u);
    }
    return i;
}

static void v1(bpf_insn *in, bpf_err want, const char *msg)
{
    bpf_u32 conf;
    bpf_err e;

    e = bpf_validate(in, 1, &conf);
    count = count + 1;
    if (e != want)
    {
        (void)fprintf(stderr, "FAIL: %s got=%d want=%d\n", msg, (int)e,
                      (int)want);
        fails = fails + 1;
    }
}

static void test_every_alu_code(void)
{
    bpf_insn in;
    int code;

    for (code = 0; code <= 15; ++code)
    {
        in = mk((bpf_byte)(((unsigned)code << 4) | BPF_CLS_ALU64), 0, 0,
                (code == BPF_ALU_END) ? 32 : 0, 0);
        v1(&in, BPF_OK, "alu64 code accepted");
    }
}

static void test_every_jmp_code(void)
{
    bpf_insn in;
    int code;

    for (code = 0; code <= 15; ++code)
    {
        bpf_err want;

        want = BPF_OK;
        if (code == BPF_JMP_CALL)
        {
            want = BPF_OK;
        }
        if (code == BPF_JMP_EXIT)
        {
            want = BPF_OK;
        }
        in = mk((bpf_byte)(((unsigned)code << 4) | BPF_CLS_JMP), 0, 0, 0, 0);
        if (code == BPF_JMP_CALL || code == BPF_JMP_EXIT)
        {
            v1(&in, want, "jmp special code");
        }
        else
        {
            v1(&in, BPF_OK, "jmp cond code");
        }
    }
}

static void test_endianness_widths(void)
{
    bpf_insn in;
    int width;

    for (width = 0; width <= 128; ++width)
    {
        in = mk(0xdc, 0, 0, width, 0);
        if (width == 16 || width == 32 || width == 64)
        {
            v1(&in, BPF_OK, "end valid width");
        }
        else
        {
            v1(&in, BPF_EEND, "end invalid width");
        }
    }
}

static void test_reg_range_all(void)
{
    bpf_insn in;
    int r;

    for (r = 0; r <= 15; ++r)
    {
        in = mk(0xb7, 0, (bpf_byte)r, 0, 0);
        if (r <= 10)
        {
            v1(&in, BPF_OK, "dst reg valid");
        }
        else
        {
            v1(&in, BPF_EREG, "dst reg invalid");
        }
        in = mk(0xbf, (bpf_byte)r, 0, 0, 0);
        if (r <= 10)
        {
            v1(&in, BPF_OK, "src reg valid");
        }
        else
        {
            v1(&in, BPF_EREG, "src reg invalid");
        }
    }
}

static void test_imm_subtypes_all(void)
{
    bpf_insn in;
    int s;

    for (s = 0; s <= 10; ++s)
    {
        in = mk(0x18, (bpf_byte)s, 0, 0, 0);
        in.is_wide = 1;
        if (s == 0)
        {
            v1(&in, BPF_OK, "imm subtype 0");
        }
        else
        {
            v1(&in, BPF_EIMM, "imm subtype unsupported");
        }
    }
}

int main(void)
{
    test_every_alu_code();
    test_every_jmp_code();
    test_endianness_widths();
    test_reg_range_all();
    test_imm_subtypes_all();
    if (fails)
    {
        (void)fprintf(stderr, "test_validate4: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_validate4 (%d cases)\n", count);
    return 0;
}
