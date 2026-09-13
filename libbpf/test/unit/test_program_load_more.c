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

/* --- raw bytecode builder --- */

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

static bpf_err load_cap(bpf_byte *buf, bpf_u32 len, bpf_u32 max_insn,
                        bpf_program **out)
{
    bpf_profile pr;

    pr.allowed = 0;
    pr.max_insn = max_insn;
    return bpf_program_load(buf, len, &pr, out);
}

static void expect1_impl(bpf_byte op, bpf_u32 src, bpf_u32 dst, int imm,
                         int off, bpf_err want, const char *name)
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
        (void)fprintf(stderr, "FAIL(%d want): %s\n", want, name);
        fails = fails + 1;
    }
}

static void expect1_dst(bpf_byte op, bpf_u32 dst, bpf_err want,
                        const char *name)
{
    expect1_impl(op, 0, dst, 0, 0, want, name);
}

static void expect1_src(bpf_byte op, bpf_u32 src, bpf_err want,
                        const char *name)
{
    expect1_impl(op, src, 1, 0, 0, want, name);
}

static void expect1(bpf_byte op, bpf_u32 src, bpf_u32 dst, int imm, int off,
                    bpf_err want, const char *name)
{
    expect1_impl(op, src, dst, imm, off, want, name);
}

/* ------------------------------------------------------------------ */
/* Size and allocation arithmetic. */

static void test_bad_lengths(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 l;

    for (l = 0; l < 64u; ++l)
    {
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, l, &p);
        /* Only a multiple-of-8, nonzero length can possibly succeed; lengths
         * 1..7 and 0 must be rejected. */
        if (l < 8u)
        {
            ck(e != BPF_OK || p == 0, "short buffer rejected or empty");
        }
        if (e == BPF_OK)
        {
            bpf_program_destroy(p);
        }
    }
    /* Explicitly non-multiple-of-8 lengths are always rejected. */
    for (l = 1u; l < 8u; ++l)
    {
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, l, &p);
        ck(e != BPF_OK, "non-multiple-of-8 length rejected");
    }
}

static void test_empty_buffer(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;

    cases = cases + 1;
    p = 0;
    e = load_raw(buf, 0, &p);
    ck(e == BPF_EEMPTY && p == 0, "empty buffer is EEMPTY");
}

static void test_truncated_wide_lengths(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 l;

    /* A wide instruction at the front needs 16 bytes. Lengths of 8 (multiple
     * of 8) must be rejected as a truncated wide. */
    for (l = 8u; l < 16u; l = l + 8u)
    {
        cases = cases + 1;
        o = 0;
        basic(buf, &o, 0x18, 0, 1, 42, 0);
        p = 0;
        e = load_raw(buf, l, &p);
        ck(e != BPF_OK, "wide cut to 8 bytes rejected");
    }
    /* A valid wide followed by a torn second instruction is rejected. */
    cases = cases + 1;
    o = 0;
    wide(buf, &o, 1, 42, 0); /* 16 bytes */
    basic(buf, &o, 0xb7, 0, 2, 7, 0);
    p = 0;
    e = load_raw(buf, o - 4, &p);
    ck(e != BPF_OK, "buffer cut inside a trailing basic instruction rejected");
    /* The full buffer loads. */
    cases = cases + 1;
    o = 0;
    wide(buf, &o, 1, 42, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK && p != 0, "wide + exit loads");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

static void test_max_insn_cap(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);
    basic(buf, &o, 0xb7, 0, 0, 2, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_cap(buf, o, 1, &p);
    ck(e == BPF_ECOUNT && p == 0, "max_insn cap of 1 rejects 3 insns");
    cases = cases + 1;
    p = 0;
    e = load_cap(buf, o, 3, &p);
    ck(e == BPF_OK, "max_insn cap of 3 accepts 3 insns");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* ------------------------------------------------------------------ */
/* Exact conformance reporting. */

typedef struct conf_row
{
    bpf_byte op;
    bpf_byte src;
    bpf_byte dst;
    int imm;
    int off;
    bpf_u32 want;
} conf_row;

static const conf_row conf_rows[] = {
    /* ALU32 arithmetic -> base32 */
    {0x04, 0, 1, 0, 0, BPF_CONF_BASE32},
    {0x14, 0, 1, 0, 0, BPF_CONF_BASE32},
    /* ALU64 arithmetic -> base64 (folds base32) */
    {0x07, 0, 1, 0, 0, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    {0x17, 0, 1, 0, 0, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    {0xb7, 0, 1, 0, 0, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    /* JMP -> base64 (folds base32); self-loop so it resolves alone */
    {0x15, 0, 1, 0, -1, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    /* JMP32 -> base32; self-loop so it resolves alone */
    {0x16, 0, 1, 0, -1, BPF_CONF_BASE32},
    /* 32-bit divide -> divmul32 (+base32) */
    {0x34, 0, 1, 0, 0, BPF_CONF_DIVMUL32 | BPF_CONF_BASE32},
    /* 64-bit divide -> divmul64 (+ divmul32 + base64 + base32) */
    {0x37, 0, 1, 0, 0,
     BPF_CONF_DIVMUL64 | BPF_CONF_DIVMUL32 | BPF_CONF_BASE64 | BPF_CONF_BASE32},
    /* 64-bit byte swap -> base64 (imm is the swap width) */
    {0xdc, 0, 1, 64, 0, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    /* byte/word memory is base32 */
    {0x71, 0, 1, 0, 0, BPF_CONF_BASE32},
    /* double-word memory -> base64 */
    {0x79, 0, 1, 0, 0, BPF_CONF_BASE64 | BPF_CONF_BASE32},
    /* atomic32 */
    {0xc3, 1, 1, 0, 0, BPF_CONF_ATOMIC32 | BPF_CONF_BASE32},
    /* atomic64 */
    {0xdb, 1, 1, 0, 0,
     BPF_CONF_ATOMIC64 | BPF_CONF_ATOMIC32 | BPF_CONF_BASE64 |
         BPF_CONF_BASE32}};

static void test_conformance_exact(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 k;

    for (k = 0u; k < sizeof(conf_rows) / sizeof(conf_rows[0]); ++k)
    {
        const conf_row *r;

        r = &conf_rows[k];
        o = 0;
        basic(buf, &o, r->op, r->src, r->dst, r->imm, r->off);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e != BPF_OK)
        {
            ck(0, "conformance instruction loads");
            continue;
        }
        ck(bpf_program_conformance(p) == r->want, "exact conformance bits");
        bpf_program_destroy(p);
    }
}

/* ------------------------------------------------------------------ */
/* Register range matrix. */

static void test_register_range_matrix(void)
{
    bpf_u32 r;

    for (r = 11u; r <= 15u; ++r)
    {
        expect1_dst(0xb7, r, BPF_EREG, "dst > r10 rejected");
    }
    for (r = 11u; r <= 15u; ++r)
    {
        expect1_src(0x0f, r, BPF_EREG, "src > r10 rejected");
    }
}

/* ------------------------------------------------------------------ */
/* r10 write matrix across ALU/ALU64 codes. */

static void test_r10_write_alu_matrix(void)
{
    bpf_u32 code;
    bpf_u32 cls;

    for (cls = 0u; cls < 2u; ++cls)
    {
        bpf_u32 base;

        base = BPF_CLS_ALU;
        if (cls)
        {
            base = BPF_CLS_ALU64;
        }
        for (code = 0u; code <= BPF_ALU_ARSH; ++code)
        {
            cases = cases + 1;
            expect1_dst((bpf_byte)((code << 4) | base), 10u, BPF_ER10,
                        "ALU/ALU64 dst r10 rejected");
        }
    }
    /* load-destination r10 */
    expect1_dst(0x61, 10u, BPF_ER10, "ldx dst r10 rejected");
    expect1_dst(0x79, 10u, BPF_ER10, "ldxdw dst r10 rejected");
    expect1_dst(0x91, 10u, BPF_ER10, "ldxsx dst r10 rejected");
}

/* ------------------------------------------------------------------ */
/* Atomic operation acceptance matrix. */

static void test_atomic_accept(void)
{
    bpf_byte size;
    bpf_u32 op;

    for (size = 0u; size < 2u; ++size)
    {
        bpf_byte atom_op;

        atom_op = 0xC3; /* atomic W */
        if (size)
        {
            atom_op = 0xDB; /* atomic DW */
        }
        for (op = 0u; op <= 0xF0u; op = op + 0x10u)
        {
            bpf_byte base;

            base = 0;
            if (op == 0x00u)
            {
                base = 1;
            }
            if (op == 0x40u)
            {
                base = 1;
            }
            if (op == 0x50u)
            {
                base = 1;
            }
            if (op == 0xA0u)
            {
                base = 1;
            }
            if (op == 0xE0u)
            {
                base = 1;
            }
            if (op == 0xF0u)
            {
                base = 1;
            }
            if (base)
            {
                expect1(atom_op, 1, 1, (int)op, 0, BPF_OK, "atomic op ok");
                expect1(atom_op, 1, 1, (int)(op | 0x01u), 0, BPF_OK,
                        "atomic op fetch ok");
            }
            else
            {
                expect1(atom_op, 1, 1, (int)op, 0, BPF_EUNSUP,
                        "atomic bad op rejected");
            }
        }
    }
}

static void test_atomic_r10_base_ok(void)
{
    /* Using r10 as the base (dst) of a non-fetch atomic is allowed. */
    expect1(0xc3, 1, 10, 0x00, 0, BPF_OK, "atomic over r10 base allowed");
}

/* ------------------------------------------------------------------ */
/* Slot accounting with multiple wide instructions. */

static void test_multiple_wides_slots(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;
    bpf_u32 k;

    o = 0;
    wide(buf, &o, 1, 1, 0);           /* slot0 */
    wide(buf, &o, 2, 2, 0);           /* slot2 */
    basic(buf, &o, 0xb7, 0, 0, 3, 0); /* slot4 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot5 exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "multi-wide program loads");
    if (e != BPF_OK)
    {
        return;
    }
    ck(bpf_program_count(p) == 4, "multi-wide counts 4 instructions");
    e = bpf_program_get(p, 0, &g);
    ck(e == BPF_OK && g.slot == 0 && g.is_wide == 1, "wide0 slot 0");
    e = bpf_program_get(p, 1, &g);
    ck(e == BPF_OK && g.slot == 2 && g.is_wide == 1, "wide1 slot 2");
    e = bpf_program_get(p, 2, &g);
    ck(e == BPF_OK && g.slot == 4 && g.is_wide == 0, "mov slot 4");
    e = bpf_program_get(p, 3, &g);
    ck(e == BPF_OK && g.slot == 5, "exit slot 5");
    /* bpf_program_get bounds. */
    for (k = 4u; k < 6u; ++k)
    {
        cases = cases + 1;
        e = bpf_program_get(p, k, &g);
        ck(e == BPF_EREG, "get past end is EREG");
    }
    bpf_program_destroy(p);
}

static void test_wide_then_forward_branch(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    /* [lddw r0][jeq r0,0 -> skip][mov r0,1][mov r0,2][exit] */
    o = 0;
    wide(buf, &o, 0, 42, 0);          /* slot0-1, index0 */
    basic(buf, &o, 0x15, 0, 0, 0, 1); /* slot2 jeq r0,0 +1 -> slot4, index3 */
    basic(buf, &o, 0xb7, 0, 0, 1, 0); /* slot3 mov r0,1 (skipped) index2 */
    basic(buf, &o, 0xb7, 0, 0, 2, 0); /* slot4 mov r0,2 index3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot5 exit index4 */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "wide-then-branch program loads");
    if (e == BPF_OK)
    {
        /* jeq at index1 (slot2) jumps +1 slot -> slot4 = index 3 (mov r0,2). */
        e = bpf_program_get(p, 1, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 3,
           "forward branch lands on mov r0,2");
        bpf_program_destroy(p);
    }
}

/* A program whose only instruction is a return code value with no branch is
 * acceptable to the loader (runtime would fall off the end later). */
static void test_single_instruction_program(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK && bpf_program_count(p) == 1, "single insn program loads");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* ------------------------------------------------------------------ */

/* Test the exact-width compilation contract at runtime (the compile-time
 * asserts are static; this double-checks the observed sizes). */
static void test_integer_contract(void)
{
    ck(sizeof(bpf_u64) == 8u, "bpf_u64 is 64 bits");
    ck(sizeof(bpf_i64) == 8u, "bpf_i64 is 64 bits");
    ck(sizeof(bpf_u32) == 4u, "bpf_u32 is 32 bits");
    ck(sizeof(bpf_i32) == 4u, "bpf_i32 is 32 bits");
    ck(sizeof(bpf_u16) == 2u, "bpf_u16 is 16 bits");
    ck(sizeof(bpf_byte) == 1u, "bpf_byte is 8 bits");
    cases = cases + 1;
}

int main(void)
{
    test_bad_lengths();
    test_empty_buffer();
    test_truncated_wide_lengths();
    test_max_insn_cap();
    test_conformance_exact();
    test_register_range_matrix();
    test_r10_write_alu_matrix();
    test_atomic_accept();
    test_atomic_r10_base_ok();
    test_multiple_wides_slots();
    test_wide_then_forward_branch();
    test_single_instruction_program();
    test_integer_contract();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_program_load_more: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_program_load_more (%d cases)\n", cases);
    return 0;
}
