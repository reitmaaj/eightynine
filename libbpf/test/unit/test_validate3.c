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

static void test_valid_instructions(void)
{
    bpf_insn in;

    in = mk(0xb7, 0, 0, 42, 0);
    v1(&in, BPF_OK, "mov64 K");
    in = mk(0xbf, 1, 2, 0, 0);
    v1(&in, BPF_OK, "mov64 X");
    in = mk(0x07, 0, 3, 1, 0);
    v1(&in, BPF_OK, "add64 K");
    in = mk(0x0f, 1, 3, 0, 0);
    v1(&in, BPF_OK, "add64 X");
    in = mk(0x37, 0, 0, 2, 0);
    v1(&in, BPF_OK, "div64");
    in = mk(0x97, 0, 0, 2, 1);
    v1(&in, BPF_OK, "smod64");
    in = mk(0x67, 0, 0, 8, 0);
    v1(&in, BPF_OK, "lsh64");
    in = mk(0xdc, 0, 0, 64, 0);
    v1(&in, BPF_OK, "end64");
    in = mk(0x05, 0, 0, 0, 3);
    v1(&in, BPF_OK, "ja");
    in = mk(0x15, 0, 1, 5, 2);
    v1(&in, BPF_OK, "jeq");
    in = mk(0x85, 0, 0, 7, 0);
    v1(&in, BPF_OK, "call");
    in = mk(0x95, 0, 0, 0, 0);
    v1(&in, BPF_OK, "exit");
    in = mk(0x71, 0, 0, 0, 0);
    v1(&in, BPF_OK, "ldx b");
    in = mk(0x79, 0, 0, 0, 0);
    v1(&in, BPF_OK, "ldx dw");
    in = mk(0x73, 0, 0, 0, 0);
    v1(&in, BPF_OK, "stx b");
    in = mk(0x7a, 0, 0, 5, 0);
    v1(&in, BPF_OK, "st dw imm");
    in = mk(0xC3, 2, 1, 0x00, 0);
    v1(&in, BPF_OK, "atomic w");
    in = mk(0xDB, 2, 1, 0xE1, 0);
    v1(&in, BPF_OK, "atomic dw xchg");
}

static void test_invalid_instructions(void)
{
    bpf_insn in;

    in = mk(0xb7, 0, 11, 0, 0);
    v1(&in, BPF_EREG, "dst 11");
    in = mk(0xbf, 11, 0, 0, 0);
    v1(&in, BPF_EREG, "src 11");
    in = mk(0x20, 0, 0, 0, 0);
    v1(&in, BPF_EDEPRECATED, "packet abs");
    in = mk(0x18, 3, 0, 0, 0);
    in.is_wide = 1;
    v1(&in, BPF_EIMM, "imm subtype 3");
    in = mk(0xdc, 0, 0, 24, 0);
    v1(&in, BPF_EEND, "end width 24");
    in = mk(0xdf, 0, 0, 32, 0);
    v1(&in, BPF_EEND, "alu64 end source bit");
    in = mk(0x8f, 0, 0, 0, 0);
    v1(&in, BPF_ENEG, "neg X");
    in = mk(0xb7, 0, 0, 0, 8);
    v1(&in, BPF_EMOVSX, "movsx K");
    in = mk(0xbf, 1, 0, 0, 7);
    v1(&in, BPF_EMOVSX, "movsx width 7");
    in = mk(0x85, 4, 0, 0, 0);
    v1(&in, BPF_ECALL, "call src 4");
    in = mk(0x95, 0, 0, 3, 0);
    v1(&in, BPF_EEXIT, "exit imm");
    in = mk(0x95, 1, 0, 0, 0);
    v1(&in, BPF_EEXIT, "exit src");
    in = mk(0x99, 0, 0, 0, 0);
    v1(&in, BPF_ESIZE, "memsx dw");
    in = mk(0xD3, 0, 0, 0, 0);
    v1(&in, BPF_ESIZE, "atomic b");
}

static void test_multi_instruction(void)
{
    bpf_insn in[3];
    bpf_u32 conf;
    bpf_err e;

    in[0] = mk(0xb7, 0, 0, 1, 0);
    in[1] = mk(0xb7, 0, 1, 2, 0);
    in[2] = mk(0x95, 0, 0, 0, 0);
    e = bpf_validate(in, 3, &conf);
    count = count + 1;
    if (e != BPF_OK || (conf & BPF_CONF_BASE64) == 0u)
    {
        (void)fprintf(stderr, "FAIL: multi valid\n");
        fails = fails + 1;
    }

    in[2] = mk(0xb7, 0, 12, 0, 0); /* invalid reg */
    e = bpf_validate(in, 3, &conf);
    count = count + 1;
    if (e != BPF_EREG)
    {
        (void)fprintf(stderr, "FAIL: multi invalid reg\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_valid_instructions();
    test_invalid_instructions();
    test_multi_instruction();
    if (fails)
    {
        (void)fprintf(stderr, "test_validate3: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_validate3 (%d cases)\n", count);
    return 0;
}
