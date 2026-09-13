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

static bpf_insn end(bpf_byte cls, bpf_byte source, int width)
{
    bpf_insn i;

    i.opcode =
        (bpf_byte)((BPF_ALU_END << 4) | (((unsigned)source & 1u) << 3) | cls);
    i.class = cls;
    i.code = BPF_ALU_END;
    i.source = source;
    i.mode = 0;
    i.size = 0;
    i.src_reg = 0;
    i.dst_reg = 1;
    i.offset = 0;
    i.imm = width;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static bpf_u64 run(const bpf_insn *in, bpf_u64 dst, bpf_byte *ok)
{
    bpf_regs r;
    bpf_err e;

    r.r[0] = 0;
    r.r[1] = dst;
    e = bpf_alu(&r, in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[1];
}

static void test_alu64_end(void)
{
    bpf_insn in;
    bpf_u64 r;
    bpf_byte ok;

    in = end(BPF_CLS_ALU64, 0, 64);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x8877665544332211UL, "bswap64");
    in = end(BPF_CLS_ALU64, 0, 32);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x0000000088776655UL, "bswap32 zero upper");
    in = end(BPF_CLS_ALU64, 0, 16);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x0000000000008877UL, "bswap16 zero upper");
}

static void test_alu_be(void)
{
    bpf_insn in;
    bpf_u64 r;
    bpf_byte ok;

    in = end(BPF_CLS_ALU, BPF_SRC_X, 64);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x8877665544332211UL, "be64");
    in = end(BPF_CLS_ALU, BPF_SRC_X, 32);
    r = run(&in, 0x0000000011223344UL, &ok);
    ck(ok && r == 0x44332211UL, "be32");
    in = end(BPF_CLS_ALU, BPF_SRC_X, 16);
    r = run(&in, 0x0000000000001122UL, &ok);
    ck(ok && r == 0x0000000000002211UL, "be16");
}

static void test_alu_le(void)
{
    bpf_insn in;
    bpf_u64 r;
    bpf_byte ok;

    in = end(BPF_CLS_ALU, BPF_SRC_K, 64);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x1122334455667788UL, "le64 identity");
    in = end(BPF_CLS_ALU, BPF_SRC_K, 32);
    r = run(&in, 0x0000000011223344UL, &ok);
    ck(ok && r == 0x0000000011223344UL, "le32 identity");
    in = end(BPF_CLS_ALU, BPF_SRC_K, 16);
    r = run(&in, 0x0000000000001122UL, &ok);
    ck(ok && r == 0x0000000000001122UL, "le16 identity");
}

static void test_alu_width8(void)
{
    bpf_insn in;
    bpf_u64 r;
    bpf_byte ok;

    in = end(BPF_CLS_ALU, BPF_SRC_X, 8);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x1122334455667788UL, "end width 8 identity");
    in = end(BPF_CLS_ALU64, 0, 8);
    r = run(&in, 0x1122334455667788UL, &ok);
    ck(ok && r == 0x1122334455667788UL, "end64 width 8 identity");
}

int main(void)
{
    test_alu64_end();
    test_alu_be();
    test_alu_le();
    test_alu_width8();
    if (fails)
    {
        (void)fprintf(stderr, "test_endian: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_endian\n");
    return 0;
}
