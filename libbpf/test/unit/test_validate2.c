#include <stdio.h>

#include "validate.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;

    i.opcode = op;
    i.class = (bpf_byte)(op & 0x07u);
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.mode = (bpf_byte)(op >> 5);
    i.size = (bpf_byte)((op >> 3) & 0x03u);
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static bpf_err v1(bpf_insn *in, bpf_u32 *conf)
{
    return bpf_validate(in, 1, conf);
}

static void test_movsx_widths(void)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

    /* MOVSX ALU64 widths 8,16,32 accepted with X source. */
    in = mk(0xbf, 1, 0, 0, 8);
    e = v1(&in, &conf);
    ck(e == BPF_OK, "movsx64 width 8 ok");
    in.offset = 16;
    e = v1(&in, &conf);
    ck(e == BPF_OK, "movsx64 width 16 ok");
    in.offset = 32;
    e = v1(&in, &conf);
    ck(e == BPF_OK, "movsx64 width 32 ok");

    /* MOVSX ALU (32-bit) widths 8,16 ok; 32 rejected. */
    in = mk(0xbc, 1, 0, 0, 8);
    e = v1(&in, &conf);
    ck(e == BPF_OK, "movsx32 width 8 ok");
    in.offset = 16;
    e = v1(&in, &conf);
    ck(e == BPF_OK, "movsx32 width 16 ok");
    in.offset = 32;
    e = v1(&in, &conf);
    ck(e == BPF_EMOVSX, "movsx32 width 32 rejected");

    /* Unsupported width. */
    in = mk(0xbf, 1, 0, 0, 4);
    e = v1(&in, &conf);
    ck(e == BPF_EMOVSX, "movsx width 4 rejected");
}

static void test_end_alu_variants(void)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

    /* ALU END LE, widths 16/32/64 ok. */
    in = mk(0xd4, 0, 0, 16, 0);
    e = v1(&in, &conf);
    ck(e == BPF_OK, "alu end LE 16 ok");
    in.imm = 32;
    e = v1(&in, &conf);
    ck(e == BPF_OK, "alu end LE 32 ok");
    in.imm = 64;
    e = v1(&in, &conf);
    ck(e == BPF_OK, "alu end LE 64 ok");

    /* ALU64 END reserved source must be 0. */
    in = mk(0xdf, 1, 0, 32, 0);
    e = v1(&in, &conf);
    ck(e == BPF_EEND, "alu64 end source bit rejected");

    in = mk(0xdc, 0, 0, 32, 0);
    e = v1(&in, &conf);
    ck(e == BPF_OK, "alu64 end source 0 ok");
}

static void test_memsx_dw_rejected(void)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

    /* MEMSX DW not defined. */
    in = mk(0x99, 0, 0, 0, 0); /* mode 4<<5, DW(3)<<3, LDX(1) = 0x99 */
    e = v1(&in, &conf);
    ck(e == BPF_ESIZE, "memsx DW rejected");
}

static void test_atomic_b_h_rejected(void)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

    /* ATOMIC B STX = mode6<<5 | B2<<3 | STX3 = 0xC2. */
    in = mk(0xD3, 0, 0, 0, 0);
    e = v1(&in, &conf);
    ck(e == BPF_ESIZE, "atomic B rejected");
    /* ATOMIC H STX = 0xCB. */
    in = mk(0xCB, 0, 0, 0, 0);
    e = v1(&in, &conf);
    ck(e == BPF_ESIZE, "atomic H rejected");
}

static void test_ja_forms(void)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

    in = mk(0x05, 0, 0, 0, 0); /* JA JMP */
    e = v1(&in, &conf);
    ck(e == BPF_OK, "ja jmp ok");
    in = mk(0x06, 0, 0, 5, 0); /* JA JMP32 */
    e = v1(&in, &conf);
    ck(e == BPF_OK, "ja jmp32 ok");
}

static void test_conformance_folding(void)
{
    bpf_insn in[2];
    bpf_u32 conf;
    bpf_err e;

    /* base64 implies base32 is always required. */
    in[0] = mk(0xb7, 0, 0, 0, 0); /* MOV64 ALU64 -> base64 */
    e = bpf_validate(in, 1, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_BASE64) != 0u, "conf base64");

    /* 32-bit div adds divmul32. */
    in[0] = mk(0x34, 0, 0, 0, 0);
    in[0].code = BPF_ALU_DIV;
    e = bpf_validate(in, 1, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_DIVMUL32) != 0u, "conf divmul32");

    /* 64-bit div adds divmul64. */
    in[0] = mk(0x37, 0, 0, 0, 0);
    in[0].code = BPF_ALU_DIV;
    e = bpf_validate(in, 1, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_DIVMUL64) != 0u, "conf divmul64");

    /* MUL is divmul too. */
    in[0] = mk(0x27, 0, 0, 0, 0);
    in[0].code = BPF_ALU_MUL;
    e = bpf_validate(in, 1, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_DIVMUL64) != 0u, "conf mul64");

    /* JMP32 is base32. */
    in[0] = mk(0x06, 0, 0, 0, 0);
    e = bpf_validate(in, 1, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_BASE32) != 0u &&
           (conf & BPF_CONF_BASE64) == 0u,
       "conf jmp32 base32 only");

    /* Mixed program unions groups. */
    in[0] = mk(0xb7, 0, 0, 0, 0); /* base64 */
    in[1] = mk(0xC3, 0, 0, 0, 0); /* atomic32 */
    e = bpf_validate(in, 2, &conf);
    ck(e == BPF_OK && (conf & BPF_CONF_BASE64) != 0u &&
           (conf & BPF_CONF_ATOMIC32) != 0u,
       "conf mixed union");
}

static void test_multi_error_stops(void)
{
    bpf_insn in[2];
    bpf_u32 conf;
    bpf_err e;

    in[0] = mk(0xb7, 0, 0, 0, 0); /* ok */
    in[1] = mk(0x20, 0, 0, 0, 0); /* deprecated packet */
    e = bpf_validate(in, 2, &conf);
    ck(e == BPF_EDEPRECATED, "second error surfaces");
    ck((conf & BPF_CONF_PACKET) != 0u, "packet conf set on error");
}

int main(void)
{
    test_movsx_widths();
    test_end_alu_variants();
    test_memsx_dw_rejected();
    test_atomic_b_h_rejected();
    test_ja_forms();
    test_conformance_folding();
    test_multi_error_stops();
    if (fails)
    {
        (void)fprintf(stderr, "test_validate2: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_validate2\n");
    return 0;
}
