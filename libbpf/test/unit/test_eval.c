#include <stdio.h>

#include "eval.h"

static int check(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

/* Build an ALU/ALU64 instruction. `op` is the raw opcode; a non-0 `sx` sets
 * the X (register) source by giving a src register. */
static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;

    i.opcode = op;
    i.class = (bpf_byte)(op & 0x07u);
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.mode = 0;
    i.size = 0;
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static int test_register_zero(void)
{
    bpf_regs r;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < BPF_NREG; ++i)
    {
        r.r[i] = 0xDEADBEEFDEADBEEFUL;
    }
    {
        bpf_insn in;
        in = mk(0xb7, 0, 1, 0, 0);
        (void)bpf_alu(&r, &in);
    }
    fail |= check(r.r[0] == 0xDEADBEEFDEADBEEFUL, "zero: untouched r0");
    return fail;
}

static int test_alu64_arith(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 0;
    in = mk(0xb7, 0, 1, 40, 0); /* MOV64 K r1, 40 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 40UL, "arith: mov K");

    in = mk(0x07, 0, 1, 2, 0); /* ADD64 K r1, 2 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 42UL, "arith: add K");

    in = mk(0x17, 0, 1, 10, 0); /* SUB64 K r1, 10 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 32UL, "arith: sub K");

    in = mk(0x27, 0, 1, 3, 0); /* MUL64 K r1, 3 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 96UL, "arith: mul K");

    in = mk(0xbf, 2, 1, 0, 0); /* MOV64 X r1 = r2 */
    r.r[2] = 7UL;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 7UL, "arith: mov X");

    in = mk(0x9f, 2, 1, 0, 0); /* XOR64 X r1 ^= r2 (7^7=0) */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0UL, "arith: xor X");

    r.r[1] = 0xFFFFFFFFFFFFFFFFUL;
    in = mk(0xb7, 0, 2, -1, 0); /* MOV64 K r2, -1 (sign-extended) */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[2] == 0xFFFFFFFFFFFFFFFFUL, "arith: mov -1 se");
    return fail;
}

static int test_alu32_zeroextend(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0xFFFF0000FFFF0001UL;
    in = mk(0xb4, 0, 1, 0, 0); /* MOV32 K r1, 0 => upper zeroed */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0UL, "alu32: mov32 zeroes upper");

    in = mk(0xb4, 0, 1, 0x1234, 0); /* MOV32 K r1, 0x1234 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0x1234UL, "alu32: mov32 value");
    return fail;
}

static int test_divmod_zero(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    in = mk(0xb7, 0, 1, 0, 0); /* MOV64 K r1, 0 (divisor) */
    (void)bpf_alu(&r, &in);
    r.r[1] = 100UL;
    in = mk(0x37, 0, 1, 0, 0); /* DIV64 K r1 = r1 / 0 */
    in.imm = 0;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0UL, "div0: div by zero -> 0");

    r.r[1] = 100UL;
    in = mk(0x97, 0, 1, 0, 0); /* MOD64 K r1 = r1 % 0 */
    in.imm = 0;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 100UL, "div0: mod by zero unchanged");
    return fail;
}

static int test_smod_truncated(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[1] = (bpf_u64)(bpf_i64)-13;
    r.r[2] = 3UL;
    in = mk(0x9f, 2, 1, 0, 1); /* SMOD64 X r1 = r1 s% r2 (offset 1) */
    (void)bpf_alu(&r, &in);
    fail |= check((bpf_i64)r.r[1] == -1, "smod: -13 % 3 == -1");
    return fail;
}

static int test_shift_masks(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[1] = 1UL;
    in = mk(0x67, 0, 1, 70, 0); /* LSH64 K r1, 70 => 70&63 = 6 => 64 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 64UL, "shift: 1<<(70&63) == 64");

    r.r[1] = 1UL;
    in = mk(0x64, 0, 1, 34, 0); /* LSH32 K r1, 34 => 34&31 = 2 => 4 */
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 4UL, "shift: 32-bit mask");
    return fail;
}

static int test_neg(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[1] = 5UL;
    in = mk(0x87, 0, 1, 0, 0); /* NEG64 K r1 */
    (void)bpf_alu(&r, &in);
    fail |= check((bpf_i64)r.r[1] == -5, "neg: -5");
    return fail;
}

static int test_movsx(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 0xFFFFFFFFFFFFFF80UL; /* low byte 0x80 = -128 */
    in = mk(0xbf, 2, 1, 0, 8);     /* MOVSX64 X r1, r2, width 8 */
    (void)bpf_alu(&r, &in);
    fail |= check((bpf_i64)r.r[1] == -128, "movsx: 8-bit sign extend");

    r.r[2] = 0x0000000000008000UL; /* low 16 bit 0x8000 = -32768 */
    in.offset = 16;
    (void)bpf_alu(&r, &in);
    fail |= check((bpf_i64)r.r[1] == -32768, "movsx: 16-bit sign extend");
    return fail;
}

static int test_end(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[1] = 0x1122334455667788UL;
    in = mk(0xdc, 0, 1, 64, 0); /* ALU64 END (imm 64) */
    in.code = BPF_ALU_END;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0x8877665544332211UL, "end: bswap64");

    r.r[1] = 0x1122334455667788UL;
    in.imm = 32;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0x0000000088776655UL, "end: bswap32 zero upper");

    r.r[1] = 0x1122334455667788UL;
    in.imm = 16;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0x0000000000008877UL, "end: bswap16 zero upper");
    return fail;
}

static int test_end_be(void)
{
    bpf_regs r;
    bpf_insn in;
    int fail;

    fail = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[1] = 0x11223344UL;
    in = mk(0xd4, 0, 1, 32, 0); /* ALU END (class 4), source BE(1) */
    in.code = BPF_ALU_END;
    in.source = BPF_SRC_X;
    (void)bpf_alu(&r, &in);
    fail |= check(r.r[1] == 0x44332211UL, "endbe: bswap32 BE");
    return fail;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_register_zero();
    fail |= test_alu64_arith();
    fail |= test_alu32_zeroextend();
    fail |= test_divmod_zero();
    fail |= test_smod_truncated();
    fail |= test_shift_masks();
    fail |= test_neg();
    fail |= test_movsx();
    fail |= test_end();
    fail |= test_end_be();
    if (fail)
    {
        (void)fprintf(stderr, "test_eval: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_eval\n");
    return 0;
}
