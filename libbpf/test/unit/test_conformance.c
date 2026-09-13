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

static void check_prog(const bpf_insn *ins, bpf_u32 n, bpf_u32 want,
                       const char *msg)
{
    bpf_u32 conf;
    bpf_err e;

    e = bpf_validate(ins, n, &conf);
    count = count + 1;
    if (e != BPF_OK)
    {
        (void)fprintf(stderr, "FAIL: %s err=%d\n", msg, (int)e);
        fails = fails + 1;
        return;
    }
    if (conf != want)
    {
        (void)fprintf(stderr, "FAIL: %s conf=%x want=%x\n", msg, conf, want);
        fails = fails + 1;
    }
}

static void test_single_insn(void)
{
    bpf_insn in[1];

    in[0] = mk(0xb4, 0, 0, 0, 0); /* MOV32 ALU -> base32 */
    check_prog(in, 1, BPF_CONF_BASE32, "mov32");

    in[0] = mk(0xb7, 0, 0, 0, 0); /* MOV64 -> base32|base64 */
    check_prog(in, 1, BPF_CONF_BASE32 | BPF_CONF_BASE64, "mov64");

    in[0] = mk(0x34, 0, 0, 0, 0); /* ALU DIV -> base32|divmul32 */
    in[0].code = BPF_ALU_DIV;
    check_prog(in, 1, BPF_CONF_BASE32 | BPF_CONF_DIVMUL32, "div32");

    in[0] = mk(0x37, 0, 0, 0, 0); /* ALU64 DIV -> base64|divmul64 */
    in[0].code = BPF_ALU_DIV;
    check_prog(in, 1,
               BPF_CONF_BASE32 | BPF_CONF_BASE64 | BPF_CONF_DIVMUL32 |
                   BPF_CONF_DIVMUL64,
               "div64");

    in[0] = mk(0xC3, 0, 0, 0, 0); /* ATOMIC W -> base32|atomic32 */
    check_prog(in, 1, BPF_CONF_BASE32 | BPF_CONF_ATOMIC32, "atomic32");

    in[0] = mk(0xDB, 0, 0, 0, 0); /* ATOMIC DW -> base64|atomic64 */
    check_prog(in, 1,
               BPF_CONF_BASE32 | BPF_CONF_BASE64 | BPF_CONF_ATOMIC32 |
                   BPF_CONF_ATOMIC64,
               "atomic64");

    in[0] = mk(0xdc, 0, 0, 64, 0); /* END 64 -> base64 */
    check_prog(in, 1, BPF_CONF_BASE32 | BPF_CONF_BASE64, "end64");

    in[0] = mk(0xdc, 0, 0, 32, 0); /* END 32 -> base32 */
    check_prog(in, 1, BPF_CONF_BASE32, "end32");

    in[0] = mk(0x79, 0, 0, 0, 0); /* LDX DW -> base64 */
    check_prog(in, 1, BPF_CONF_BASE32 | BPF_CONF_BASE64, "ldx dw");
}

static void test_mixed_program(void)
{
    bpf_insn in[4];

    in[0] = mk(0xb7, 0, 0, 0, 0); /* mov64 -> base64 */
    in[1] = mk(0x34, 0, 0, 0, 0); /* div32 -> divmul32 */
    in[1].code = BPF_ALU_DIV;
    in[2] = mk(0xC3, 0, 0, 0, 0); /* atomic32 */
    in[3] = mk(0x05, 0, 0, 0, 0); /* JA JMP -> base64 */
    check_prog(in, 4,
               BPF_CONF_BASE32 | BPF_CONF_BASE64 | BPF_CONF_DIVMUL32 |
                   BPF_CONF_ATOMIC32,
               "mixed");
}

static void test_packet_conf(void)
{
    bpf_insn in[1];
    bpf_u32 conf;
    bpf_err e;

    in[0] = mk(0x20, 0, 0, 0, 0); /* deprecated packet */
    e = bpf_validate(in, 1, &conf);
    count = count + 1;
    if (e != BPF_EDEPRECATED || (conf & BPF_CONF_PACKET) == 0u)
    {
        (void)fprintf(stderr, "FAIL: packet conf\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_single_insn();
    test_mixed_program();
    test_packet_conf();
    if (fails)
    {
        (void)fprintf(stderr, "test_conformance: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_conformance (%d cases)\n", count);
    return 0;
}
