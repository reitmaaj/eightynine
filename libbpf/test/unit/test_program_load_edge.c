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

/* Reserved continuation bytes of a wide instruction must be zero. */
static void test_wide_reserved_each_byte(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 k;

    for (k = 8u; k < 12u; ++k)
    {
        o = 0;
        wide(buf, &o, 1, 42, 0);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        buf[k] = 0xFF;
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        ck(e == BPF_ERES && p == 0, "each reserved continuation byte zero");
    }
    /* The high immediate bytes (12..15) legitimately carry the upper half. */
    o = 0;
    wide(buf, &o, 1, 0, 0xDEADBEEFu);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "wide with high immediate accepted");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* Wide immediate low/high halves are preserved across loading. */
static void test_wide_immediate_preserved(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;
    bpf_u32 lo;
    bpf_u32 hi;

    lo = 0x9ABCDEF0u;
    hi = 0x12345678u;
    o = 0;
    wide(buf, &o, 5, lo, hi);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "wide 64-bit immediate program loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 0, &g);
        ck(e == BPF_OK && (bpf_u32)g.in.imm == lo, "low immediate preserved");
        ck(e == BPF_OK && g.in.next_imm == hi, "high immediate preserved");
        bpf_program_destroy(p);
    }
}

/* A wide load into every non-r10 register, then inspect metadata. */
static void test_wide_all_regs(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 r;

    for (r = 0u; r <= 10u; ++r)
    {
        o = 0;
        if (r == 10u)
        {
            /* Loading r10 is forbidden. */
            wide(buf, &o, r, 1, 0);
            basic(buf, &o, 0x95, 0, 0, 0, 0);
            cases = cases + 1;
            p = 0;
            e = load_raw(buf, o, &p);
            ck(e == BPF_ER10, "wide load into r10 rejected");
            continue;
        }
        wide(buf, &o, r, 1, 0);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e != BPF_OK)
        {
            ck(0, "wide load into non-r10 register accepted");
            continue;
        }
        bpf_program_destroy(p);
    }
}

/* Register cross-product over a set of write-destination opcodes. */
static void test_dst_register_boundary(void)
{
    bpf_u32 d;

    for (d = 0u; d <= 12u; ++d)
    {
        bpf_err want;
        bpf_byte buf[64] = {0};
        bpf_program *p;
        bpf_err e;
        bpf_u32 o;

        want = BPF_OK;
        if (d == 10u)
        {
            want = BPF_ER10;
        }
        if (d > 10u)
        {
            want = BPF_EREG;
        }
        o = 0;
        basic(buf, &o, 0xb7, 0, d, 7, 0);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e == BPF_OK)
        {
            bpf_program_destroy(p);
        }
        ck(e == want, "dst register boundary enforced");
    }
}

/* A program needing divmul64 must be rejected by a base64-only profile. */
static void test_divmul_needs_group(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_profile pr;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x37, 0, 1, 0, 0); /* divmul64 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    pr.allowed = BPF_CONF_BASE64 | BPF_CONF_BASE32;
    pr.max_insn = 0;
    cases = cases + 1;
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_EPROF, "divmul64 needs DIVMUL64 group");
    pr.allowed = BPF_CONF_DIVMUL64 | BPF_CONF_DIVMUL32 | BPF_CONF_BASE64 |
                 BPF_CONF_BASE32;
    cases = cases + 1;
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_OK, "divmul64 allowed with DIVMUL64 group");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* Branching to the first and last instructions is valid; resolve check. */
static void test_branch_to_ends(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;
    bpf_u32 i;
    bpf_u32 n;

    /* Build a program with several instructions; branch from head to tail. */
    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0); /* slot0 mov */
    basic(buf, &o, 0xb7, 0, 1, 1, 0); /* slot1 mov */
    basic(buf, &o, 0xb7, 0, 2, 2, 0); /* slot2 mov */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot3 exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "program with several basic insns loads");
    if (e == BPF_OK)
    {
        n = bpf_program_count(p);
        ck(n == 4, "four-instruction program");
        for (i = 0u; i < n; ++i)
        {
            cases = cases + 1;
            e = bpf_program_get(p, i, &g);
            ck(e == BPF_OK && g.slot == i, "slot equals index for basic only");
        }
        bpf_program_destroy(p);
    }
}

/* A forward conditional branch to the very last instruction. */
static void test_cond_to_last(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    /* [jeq r1,0 +1][mov r0,1][exit]: target exit at slot2 */
    o = 0;
    basic(buf, &o, 0x15, 0, 1, 0, 1); /* jeq +1 */
    basic(buf, &o, 0xb7, 0, 0, 1, 0); /* mov */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "branch to last instruction loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 0, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 2,
           "branch resolves to last instruction");
        bpf_program_destroy(p);
    }
}

/* Disassembly-style walk reports resolved targets only where present. */
static void test_metadata_walk(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 5, 0); /* index0 no target */
    wide(buf, &o, 1, 9, 0);           /* index1 wide no target */
    basic(buf, &o, 0x05, 0, 0, 0, 1); /* index2 JA +1 -> exit index4 */
    basic(buf, &o, 0xb7, 0, 2, 1, 0); /* index3 mov (fallthrough) */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* index4 exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "metadata walk program loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 0, &g);
        ck(e == BPF_OK && g.has_target == 0, "mov has no target");
        e = bpf_program_get(p, 1, &g);
        ck(e == BPF_OK && g.is_wide == 1 && g.slot == 1, "wide metadata");
        e = bpf_program_get(p, 2, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 4,
           "JA resolves across wide to exit");
        bpf_program_destroy(p);
    }
}

/* A helper CALL (src_reg 0 or 2) has no in-program target and is accepted;
 * other src_reg values are rejected by structural validation. */
static void expect_call_src(bpf_u32 src, bpf_err want, const char *name)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x85, src, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
    ck(e == want, name);
}

static void test_helper_call_ids(void)
{
    expect_call_src(0u, BPF_OK, "helper call src0 accepted");
    expect_call_src(2u, BPF_OK, "helper call src2 accepted");
    expect_call_src(4u, BPF_ECALL, "helper call src4 rejected");
    expect_call_src(7u, BPF_ECALL, "helper call src7 rejected");
}

int main(void)
{
    test_wide_reserved_each_byte();
    test_wide_immediate_preserved();
    test_wide_all_regs();
    test_dst_register_boundary();
    test_divmul_needs_group();
    test_branch_to_ends();
    test_cond_to_last();
    test_metadata_walk();
    test_helper_call_ids();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_program_load_edge: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_program_load_edge (%d cases)\n", cases);
    return 0;
}
