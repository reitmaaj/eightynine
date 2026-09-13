#include <stdio.h>

#include "program.h"

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

static void e8(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    *o = *o + 1;
}

static void e16(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    buf[*o + 1] = (bpf_byte)((v >> 8) & 0xFFu);
    *o = *o + 2;
}

static void e32(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    buf[*o + 1] = (bpf_byte)((v >> 8) & 0xFFu);
    buf[*o + 2] = (bpf_byte)((v >> 16) & 0xFFu);
    buf[*o + 3] = (bpf_byte)((v >> 24) & 0xFFu);
    *o = *o + 4;
}

static void basic(bpf_byte *buf, bpf_u32 *o, bpf_u32 op, bpf_u32 src,
                  bpf_u32 dst, int imm, int off)
{
    e8(buf, o, op);
    e8(buf, o, (src << 4) | dst);
    e16(buf, o, (bpf_u32)(unsigned short)off);
    e32(buf, o, (bpf_u32)imm);
}

static void wide(bpf_byte *buf, bpf_u32 *o, bpf_u32 dst, bpf_u32 lo, bpf_u32 hi)
{
    basic(buf, o, 0x18u, 0, dst, (int)lo, 0);
    e8(buf, o, 0);
    e8(buf, o, 0);
    e16(buf, o, 0);
    e32(buf, o, hi);
}

static bpf_err load_raw(bpf_byte *buf, bpf_u32 len, bpf_program **out)
{
    bpf_profile pr;

    pr.allowed = 0;
    pr.max_insn = 0;
    return bpf_program_load(buf, len, &pr, out);
}

static void expect1(bpf_byte op, bpf_u32 src, bpf_u32 dst, int imm, int off,
                    bpf_err want, const char *name)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, op, src, dst, imm, off);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
    if (e != want)
    {
        (void)fprintf(stderr, "FAIL(%d): %s\n", want, name);
        fails = fails + 1;
    }
}

/* Every ALU/ALU64 operation code 0..0xD (K and X sources where valid) is
 * accepted; reserved codes 0xE/0xF are rejected. */
static void test_alu_code_sweep(void)
{
    bpf_u32 code;
    bpf_u32 cls;

    for (cls = 0u; cls < 2u; ++cls)
    {
        bpf_u32 base;
        bpf_u32 src;

        base = BPF_CLS_ALU;
        if (cls)
        {
            base = BPF_CLS_ALU64;
        }
        for (code = 0u; code <= 0xFu; ++code)
        {
            for (src = 0u; src <= 1u; ++src)
            {
                bpf_byte op;
                bpf_err want;

                op = (bpf_byte)((code << 4) | base | (src << 3));
                want = BPF_EUNSUP;
                if (code <= BPF_ALU_ARSH)
                {
                    want = BPF_OK;
                    if (code == BPF_ALU_NEG)
                    {
                        if (src)
                        {
                            want = BPF_ENEG;
                        }
                    }
                    if (code == BPF_ALU_MOV)
                    {
                        /* MOV imm (off 0) valid for both sources here. */
                        want = BPF_OK;
                    }
                }
                if (code == BPF_ALU_END)
                {
                    /* END needs a valid swap width as its immediate. */
                    want = BPF_EEND;
                }
                expect1(op, src, 1, 0, 0, want, "alu code sweep");
            }
        }
    }
}

/* Byte-swap widths and endianness sources. */
static void test_endian_matrix(void)
{
    bpf_u32 w;
    bpf_u32 src;

    for (w = 0u; w <= 128u; w = w + 16u)
    {
        for (src = 0u; src <= 1u; ++src)
        {
            bpf_err want;

            want = BPF_EEND;
            if (w == 16 || w == 32 || w == 64)
            {
                want = BPF_OK;
            }
            expect1(0xdc, src, 1, (int)w, 0, want, "endian width");
        }
    }
}

/* MOVSX width + source validity. */
static void test_movsx_matrix(void)
{
    bpf_u32 w;
    bpf_u32 cls;

    for (w = 8u; w <= 64u; w = w + 8u)
    {
        for (cls = 0u; cls < 2u; ++cls)
        {
            bpf_byte op;
            bpf_err want;

            op = 0xBF;
            if (cls)
            {
                op = 0xBC;
            }
            want = BPF_EMOVSX;
            if (cls == 0u)
            {
                if (w == 8 || w == 16 || w == 32)
                {
                    want = BPF_OK;
                }
            }
            else
            {
                if (w == 8 || w == 16)
                {
                    want = BPF_OK;
                }
            }
            expect1(op, 1, 1, 0, (int)w, want, "movsx width");
        }
    }
}

/* Memory access size variants are accepted per class. */
static void test_mem_sizes(void)
{
    expect1(0x61, 0, 1, 0, 0, BPF_OK, "ldx w");
    expect1(0x69, 0, 1, 0, 0, BPF_OK, "ldx h");
    expect1(0x71, 0, 1, 0, 0, BPF_OK, "ldx b");
    expect1(0x79, 0, 1, 0, 0, BPF_OK, "ldx dw");
    expect1(0x63, 1, 1, 0, 0, BPF_OK, "stx w");
    expect1(0x6b, 1, 1, 0, 0, BPF_OK, "stx h");
    expect1(0x73, 1, 1, 0, 0, BPF_OK, "stx b");
    expect1(0x7b, 1, 1, 0, 0, BPF_OK, "stx dw");
    expect1(0x62, 0, 1, 5, 0, BPF_OK, "st w imm");
    expect1(0x6a, 0, 1, 5, 0, BPF_OK, "st h imm");
    expect1(0x72, 0, 1, 5, 0, BPF_OK, "st b imm");
    expect1(0x7a, 0, 1, 5, 0, BPF_OK, "st dw imm");
    expect1(0x81, 0, 1, 0, 0, BPF_OK, "ldxsx w");
    expect1(0x89, 0, 1, 0, 0, BPF_OK, "ldxsx h");
    expect1(0x91, 0, 1, 0, 0, BPF_OK, "ldxsx b");
}

/* Atomic operation matrix for word and double-word widths. */
static void test_atomic_full(void)
{
    bpf_u32 width;

    for (width = 0u; width < 2u; ++width)
    {
        bpf_byte base;
        bpf_u32 o;

        base = 0xC3;
        if (width)
        {
            base = 0xDB;
        }
        for (o = 0x00u; o <= 0xF0u; o = o + 0x10u)
        {
            int i;
            bpf_err want;

            want = BPF_EUNSUP;
            if (o == 0x00u || o == 0x40u || o == 0x50u || o == 0xA0u ||
                o == 0xE0u || o == 0xF0u)
            {
                want = BPF_OK;
            }
            for (i = 0; i < 2; ++i)
            {
                int imm;

                imm = (int)o + i;
                expect1(base, 1, 1, imm, 0, want, "atomic op");
            }
        }
    }
}

/* Wide loads throughout a register range; r10 rejected. */
static void test_wide_registers(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 r;

    for (r = 0u; r <= 10u; ++r)
    {
        bpf_err want;

        want = BPF_OK;
        if (r == 10u)
        {
            want = BPF_ER10;
        }
        o = 0;
        wide(buf, &o, r, 0, 0);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e == BPF_OK)
        {
            bpf_program_destroy(p);
        }
        if (e != want)
        {
            (void)fprintf(stderr, "FAIL: wide into r%u\n", r);
            fails = fails + 1;
        }
    }
}

/* Backward branch to the very first instruction is accepted. */
static void test_backward_to_start(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);  /* slot0 mov */
    basic(buf, &o, 0xb7, 0, 1, 2, 0);  /* slot1 mov */
    basic(buf, &o, 0x05, 0, 0, 0, -3); /* slot2 JA -3 -> slot0 (index0) */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "backward branch to start loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 2, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 0,
           "JA resolves back to index 0");
        bpf_program_destroy(p);
    }
}

/* JMP32 JA uses the 32-bit immediate for its offset. */
static void test_jmp32_ja_imm(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    /* [mov r0,1][mov r1,2][jmp32 JA imm=1 -> exit at slot3? +1]
     * Layout all basic: index0..index3. JA at index2 imm +1 -> index3 exit. */
    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);
    basic(buf, &o, 0xb7, 0, 1, 2, 0);
    basic(buf, &o, 0x06, 0, 0, 0, 0); /* JMP32 JA imm 0 -> exit */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "jmp32 JA loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 2, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 3,
           "jmp32 JA resolves by imm to exit");
        bpf_program_destroy(p);
    }
}

/* A round trip: many wides interspersed with branches resolve consistently. */
static void test_round_trip_branches(void)
{
    bpf_byte buf[128] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;
    bpf_u32 i;
    bpf_u32 n;

    o = 0;
    wide(buf, &o, 1, 1, 0);           /* index0  slot0-1 */
    basic(buf, &o, 0xb7, 0, 0, 1, 0); /* index1  slot2 */
    wide(buf, &o, 2, 2, 0);           /* index2  slot3-4 */
    basic(buf, &o, 0x05, 0, 0, 0, 1); /* index3  slot5 JA +1 -> slot7 */
    basic(buf, &o, 0xb7, 0, 3, 3, 0); /* index4  slot6 (skipped) */
    basic(buf, &o, 0xb7, 0, 4, 4, 0); /* index5  slot7 mov (target) */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* index6  slot8 exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "round-trip program loads");
    if (e != BPF_OK)
    {
        return;
    }
    n = bpf_program_count(p);
    ck(n == 7, "round-trip counts 7 instructions");
    e = bpf_program_get(p, 3, &g);
    ck(e == BPF_OK && g.has_target == 1 && g.target == 5,
       "JA crosses wide to index5");
    for (i = 0u; i < n; ++i)
    {
        cases = cases + 1;
        e = bpf_program_get(p, i, &g);
        ck(e == BPF_OK, "metadata readable for every index");
    }
    bpf_program_destroy(p);
}

/* Reserved continuation: a nonzero opcode/regs/offset byte in the second slot
 * is rejected wherever it appears. */
static void test_wide_second_slot_bad(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 k;

    for (k = 8u; k < 12u; ++k)
    {
        o = 0;
        wide(buf, &o, 1, 7, 0);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        buf[k] = 0x55;
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        ck(e == BPF_ERES && p == 0, "bad continuation byte rejected");
    }
}

/* Conformance groups require their own profile bits. */
static void test_group_granularity(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_profile pr;
    bpf_u32 o;

    /* divmul32 program under a base-only profile is rejected. */
    o = 0;
    basic(buf, &o, 0x34, 0, 1, 0, 0); /* 32-bit div */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    pr.allowed = BPF_CONF_BASE32;
    pr.max_insn = 0;
    cases = cases + 1;
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_EPROF, "divmul32 needs DIVMUL32");

    /* atomic program under base-only is rejected. */
    o = 0;
    basic(buf, &o, 0xc3, 1, 1, 0, 0); /* atomic w */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    pr.allowed = BPF_CONF_BASE32;
    cases = cases + 1;
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_EPROF, "atomic needs ATOMIC group");

    /* atomic allowed once ATOMIC32 granted (with its base). */
    pr.allowed = BPF_CONF_ATOMIC32 | BPF_CONF_BASE64 | BPF_CONF_BASE32;
    cases = cases + 1;
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_OK, "atomic32 accepted under ATOMIC32 profile");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* Register numbers 10 and 11 split r10-write from out-of-range rejection. */
static void test_reg_boundary_sources(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 r;

    for (r = 9u; r <= 12u; ++r)
    {
        bpf_err want;

        want = BPF_OK;
        if (r == 11u || r == 12u)
        {
            want = BPF_EREG;
        }
        o = 0;
        basic(buf, &o, 0x0f, r, 1, 0, 0); /* ALU64 ADD with src r */
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e == BPF_OK)
        {
            bpf_program_destroy(p);
        }
        if (e != want)
        {
            (void)fprintf(stderr, "FAIL: src reg %u\n", r);
            fails = fails + 1;
        }
    }
}

int main(void)
{
    test_alu_code_sweep();
    test_endian_matrix();
    test_movsx_matrix();
    test_mem_sizes();
    test_atomic_full();
    test_wide_registers();
    test_backward_to_start();
    test_jmp32_ja_imm();
    test_round_trip_branches();
    test_wide_second_slot_bad();
    test_group_granularity();
    test_reg_boundary_sources();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_program_load_more2: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_program_load_more2 (%d cases)\n", cases);
    return 0;
}
