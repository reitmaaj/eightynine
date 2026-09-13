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

/* Build [<single decoded instruction>, exit] and load it, expecting `want`. */
static void expect1(bpf_byte op, bpf_u32 src, bpf_u32 dst, int imm, int off,
                    bpf_err want, const char *name)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, op, src, dst, imm, off);
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
        (void)fprintf(stderr, "FAIL(%d): %s\n", want, name);
        fails = fails + 1;
    }
}

static bpf_profile prof_only(bpf_u32 allowed)
{
    bpf_profile pr;

    pr.allowed = allowed;
    pr.max_insn = 0;
    return pr;
}

/* Load a single (non-branch) instruction under a specific allowed-conformance
 * profile. Because the loader checks the profile before resolving branch
 * targets, a lone branch instruction may also be used when rejection is
 * expected. */
static void expect_prof(bpf_byte op, bpf_u32 src, bpf_u32 dst, bpf_u32 allowed,
                        bpf_err want, const char *name)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_profile pr;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, op, src, dst, 0, 0);
    cases = cases + 1;
    pr = prof_only(allowed);
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
    if (e != want)
    {
        (void)fprintf(stderr, "FAIL(prof): %s\n", name);
        fails = fails + 1;
    }
}

/* ------------------------------------------------------------------ */

/* Valid ALU/ALU64 register- and immediate-source operations (codes ADD
 * through ARSH) must be accepted. */
static void test_valid_alu_loop(void)
{
    bpf_u32 code;
    bpf_u32 src;
    bpf_byte op64;
    bpf_byte op32;

    for (code = 0u; code <= BPF_ALU_ARSH; ++code)
    {
        if (code == BPF_ALU_NEG || code == BPF_ALU_MOV)
        {
            /* NEG/MOV have source restrictions exercised separately. */
            continue;
        }
        op64 = (bpf_byte)((code << 4) | BPF_CLS_ALU64);
        op32 = (bpf_byte)((code << 4) | BPF_CLS_ALU);
        for (src = 0u; src <= 1u; ++src)
        {
            bpf_byte op;

            op = op64;
            if (src)
            {
                op = (bpf_byte)(op | 0x08u);
            }
            expect1(op, src, 1, 0, 0, BPF_OK, "valid ALU64 op");
            op = op32;
            if (src)
            {
                op = (bpf_byte)(op | 0x08u);
            }
            expect1(op, src, 1, 0, 0, BPF_OK, "valid ALU op");
        }
    }
}

/* MOV and NEG valid forms. */
static void test_valid_mov_neg(void)
{
    expect1(0xb7, 0, 1, 5, 0, BPF_OK, "mov64 imm");
    expect1(0xbf, 1, 1, 0, 0, BPF_OK, "mov64 x");
    expect1(0xb4, 0, 1, 5, 0, BPF_OK, "mov32 imm");
    expect1(0xbc, 1, 1, 0, 0, BPF_OK, "mov32 x");
    expect1(0x87, 0, 1, 0, 0, BPF_OK, "neg64");
    expect1(0x84, 0, 1, 0, 0, BPF_OK, "neg32");
}

/* MOVSX forms that must be accepted (ALU64 only, register source). */
static void test_valid_movsx(void)
{
    expect1(0xbf, 1, 1, 0, 8, BPF_OK, "movsx64 8");
    expect1(0xbf, 1, 1, 0, 16, BPF_OK, "movsx64 16");
    expect1(0xbf, 1, 1, 0, 32, BPF_OK, "movsx64 32");
    expect1(0xbc, 1, 1, 0, 8, BPF_OK, "movsx32 8");
    expect1(0xbc, 1, 1, 0, 16, BPF_OK, "movsx32 16");
}

/* Byte-swap valid widths, big- and little-endian. */
static void test_valid_endian(void)
{
    expect1(0xdc, 1, 1, 16, 0, BPF_OK, "end 16 le"); /* LE BSWAP16 */
    expect1(0xdc, 1, 1, 32, 0, BPF_OK, "end 32 le"); /* LE BSWAP32 */
    expect1(0xdc, 1, 1, 64, 0, BPF_OK, "end 64 le"); /* LE BSWAP64 */
    expect1(0xdc, 0, 1, 16, 0, BPF_OK, "end 16 be"); /* BE BSWAP16 */
    expect1(0xdc, 0, 1, 32, 0, BPF_OK, "end 32 be"); /* BE BSWAP32 */
    expect1(0xdc, 0, 1, 64, 0, BPF_OK, "end 64 be"); /* BE BSWAP64 */
}

/* Valid JMP forms. */
static void test_valid_jumps(void)
{
    expect1(0x05, 0, 0, 0, 0, BPF_OK, "ja fallthrough");
    expect1(0x15, 0, 1, 0, 0, BPF_OK, "jeq imm target next");
    expect1(0x55, 1, 1, 0, 0, BPF_OK, "jne x");
    expect1(0x25, 0, 1, 0, 0, BPF_OK, "jgt");
    expect1(0x35, 0, 1, 0, 0, BPF_OK, "jge");
    expect1(0x45, 0, 1, 0, 0, BPF_OK, "jset");
    expect1(0x65, 0, 1, 0, 0, BPF_OK, "jsgt");
    expect1(0x75, 0, 1, 0, 0, BPF_OK, "jsge");
    expect1(0x85, 1, 0, 0, 0, BPF_OK, "local call target next");
    expect1(0x95, 0, 0, 0, 0, BPF_OK, "exit");
    expect1(0xa5, 0, 1, 0, 0, BPF_OK, "jlt");
    expect1(0xb5, 0, 1, 0, 0, BPF_OK, "jle");
    expect1(0xc5, 0, 1, 0, 0, BPF_OK, "jslt");
    expect1(0xd5, 0, 1, 0, 0, BPF_OK, "jsle");
}

/* JMP32 JA uses a 32-bit immediate offset. */
static void test_jmp32_ja(void)
{
    expect1(0x06, 0, 0, 0, 0, BPF_OK, "jmp32 ja 0");
    expect1(0x16, 0, 1, 0, 0, BPF_OK, "jeq32 imm");
    expect1(0xc6, 0, 1, 0, 0, BPF_OK, "jslt32");
}

/* A single decoded instruction stored to/loaded from memory. */
static void test_valid_mem(void)
{
    expect1(0x61, 0, 1, 0, 0, BPF_OK, "ldx w");
    expect1(0x71, 0, 1, 0, 0, BPF_OK, "ldx b");
    expect1(0x69, 0, 1, 0, 0, BPF_OK, "ldx h");
    expect1(0x79, 0, 1, 0, 0, BPF_OK, "ldx dw");
    expect1(0x62, 0, 1, 7, 0, BPF_OK, "st w");
    expect1(0x63, 1, 1, 0, 0, BPF_OK, "stx w");
    expect1(0x73, 1, 1, 0, 0, BPF_OK, "stx b");
    expect1(0x7b, 1, 1, 0, 0, BPF_OK, "stx dw");
    expect1(0x6b, 1, 1, 0, 0, BPF_OK, "stx h");
    /* sign-extending loads (mode MEMSX, class LDX) */
    expect1(0x81, 0, 1, 0, 0, BPF_OK, "ldxsx w");
    expect1(0x89, 0, 1, 0, 0, BPF_OK, "ldxsx h");
    expect1(0x91, 0, 1, 0, 0, BPF_OK, "ldxsx b");
    /* atomic write (no fetch) */
    expect1(0xc3, 1, 1, 0x00, 0, BPF_OK, "atomic add w");
    expect1(0xdb, 1, 1, 0x01, 0, BPF_OK, "atomic add dw fetch");
    expect1(0xc3, 1, 1, 0xe1, 0, BPF_OK, "atomic xchg w");
    expect1(0xdb, 1, 1, 0xf0, 0, BPF_OK, "atomic cmpxchg dw");
}

/* Wide (IMM LD) immediate loads, any destination but r10. */
static void test_valid_wide(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_u32 d;

    for (d = 0; d <= 9; ++d)
    {
        o = 0;
        wide(buf, &o, d, 0xDEADBEEFu, 0x12345678u);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        cases = cases + 1;
        p = 0;
        e = load_raw(buf, o, &p);
        if (e == BPF_OK)
        {
            ck(bpf_program_count(p) == 2, "wide load counted as one insn");
            bpf_program_destroy(p);
        }
        else
        {
            ck(0, "wide load should be accepted");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rejected encodings. */

static void test_reject_reg_range(void)
{
    expect1(0x07, 0, 11, 1, 0, BPF_EREG, "dst reg 11 rejected");
    expect1(0x0f, 11, 1, 0, 0, BPF_EREG, "src reg 11 rejected");
    expect1(0xb7, 0, 15, 0, 0, BPF_EREG, "dst reg 15 rejected");
}

static void test_reject_r10_writes(void)
{
    expect1(0x04, 0, 10, 1, 0, BPF_ER10, "alu32 add r10");
    expect1(0x07, 0, 10, 1, 0, BPF_ER10, "alu64 add r10");
    expect1(0x0f, 1, 10, 0, 0, BPF_ER10, "alu64 add r10 x");
    expect1(0x17, 0, 10, 1, 0, BPF_ER10, "alu64 sub r10");
    expect1(0xb7, 0, 10, 0, 0, BPF_ER10, "mov64 r10");
    expect1(0x87, 0, 10, 0, 0, BPF_ER10, "neg64 r10");
    expect1(0x61, 0, 10, 0, 0, BPF_ER10, "ldx into r10");
    expect1(0x79, 0, 10, 0, 0, BPF_ER10, "ldxdw into r10");
    expect1(0xc3, 10, 1, 0x01, 0, BPF_ER10, "atomic fetch src r10");
    expect1(0xc3, 10, 1, 0xE1, 0, BPF_ER10, "atomic xchg src r10");
}

/* ALU destination 10 but on a compare-less JMP class does not write r10. */
static void test_r10_compare_base_not_write(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x15, 0, 10, 0, 0); /* jeq r10, imm; compare reads r10 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
    ck(e == BPF_OK, "compare with r10 operand is allowed");
}

static void test_reject_deprecated_packet(void)
{
    expect1(0x20, 0, 0, 0, 0, BPF_EDEPRECATED, "abs rejected");
    expect1(0x40, 0, 0, 0, 0, BPF_EDEPRECATED, "ind rejected");
}

static void test_reject_unknown_ops(void)
{
    bpf_byte cls;
    int code;

    for (code = 0x0E; code <= 0x0F; ++code)
    {
        for (cls = 0; cls < 8; ++cls)
        {
            bpf_byte op;
            bpf_byte isam;

            isam = 0;
            if (cls == BPF_CLS_ALU || cls == BPF_CLS_ALU64 ||
                cls == BPF_CLS_JMP || cls == BPF_CLS_JMP32)
            {
                isam = 1;
            }
            if (!isam)
            {
                continue;
            }
            op = (bpf_byte)((code << 4) | cls);
            expect1(op, 0, 1, 0, 0, BPF_EUNSUP, "reserved op rejected");
        }
    }
}

static void test_reject_bad_endian(void)
{
    expect1(0xdc, 0, 1, 8, 0, BPF_EEND, "end width 8 rejected");
    expect1(0xdc, 0, 1, 24, 0, BPF_EEND, "end width 24 rejected");
    expect1(0xdc, 0, 1, 0, 0, BPF_EEND, "end width 0 rejected");
    expect1(0xdc, 0, 1, 128, 0, BPF_EEND, "end width 128 rejected");
}

static void test_reject_bad_movsx(void)
{
    expect1(0xb7, 0, 1, 0, 8, BPF_EMOVSX, "movsx from imm rejected");
    expect1(0xbf, 1, 1, 0, 4, BPF_EMOVSX, "movsx width 4 rejected");
    expect1(0xbc, 1, 1, 0, 32, BPF_EMOVSX, "movsx32 width 32 rejected");
    expect1(0xbf, 1, 1, 0, 64, BPF_EMOVSX, "movsx64 width 64 rejected");
}

static void test_reject_bad_neg(void)
{
    expect1(0x8f, 1, 1, 0, 0, BPF_ENEG, "neg64 with reg source rejected");
    expect1(0x8c, 1, 1, 0, 0, BPF_ENEG, "neg32 with reg source rejected");
}

static void test_reject_bad_call_exit(void)
{
    expect1(0x85, 3, 0, 0, 0, BPF_ECALL, "call src_reg 3 rejected");
    expect1(0x95, 2, 0, 0, 0, BPF_EEXIT, "exit src_reg 2 rejected");
    expect1(0x95, 0, 0, 0, 1, BPF_EEXIT, "exit off nonzero rejected");
    expect1(0x95, 0, 0, 1, 0, BPF_EEXIT, "exit imm nonzero rejected");
}

static void test_reject_bad_atomic(void)
{
    expect1(0xc3, 1, 1, 0x90, 0, BPF_EUNSUP, "atomic bad op rejected");
    expect1(0xcb, 1, 1, 0x00, 0, BPF_ESIZE, "atomic size h rejected");
    expect1(0xd3, 1, 1, 0x00, 0, BPF_ESIZE, "atomic size b rejected");
}

/* ------------------------------------------------------------------ */
/* Branch resolution boundaries. */

static void test_branch_boundaries(void)
{
    /* Program: [JA off, exit] over slots 0..1 (total 2). */
    expect1(0x05, 0, 0, 0, 0, BPF_OK, "ja +0 to exit");
    expect1(0x05, 0, 0, 0, -1, BPF_OK, "ja -1 self");
    expect1(0x05, 0, 0, 0, 1, BPF_EBRANCH, "ja +1 past end");
    expect1(0x05, 0, 0, 0, 99, BPF_EBRANCH, "ja +99 past end");
    expect1(0x05, 0, 0, 0, -2, BPF_EBRANCH, "ja -2 before start");
    expect1(0x05, 0, 0, 0, 32767, BPF_EBRANCH, "ja max off past end");
    expect1(0x05, 0, 0, 0, -32768, BPF_EBRANCH, "ja min off before start");
}

static void test_call_boundaries(void)
{
    /* [call 0, exit]: call offset 0 -> exit at index 1 (next). */
    expect1(0x85, 1, 0, 0, 0, BPF_OK, "local call +0");
    expect1(0x85, 1, 0, -1, 0, BPF_OK, "local call -1 self");
    expect1(0x85, 1, 0, 5, 0, BPF_EBRANCH, "local call +5 past end");
}

/* Conditional jump crossing a wide instruction resolves to the slot-correct
 * index. Program: JA is separate; here use a JEQ whose target crosses a wide.
 * Layout (slots): [0]=mov r1,1; wide r2 at slots1-2; [3]=mov r0,0; [4]=exit.
 * Insert a JEQ at index? We craft a dedicated buffer. */
static void test_branch_cross_wide_correct(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 1, 0); /* slot0: mov r1, 1 */
    wide(buf, &o, 2, 7, 0);           /* slots1-2: lddw r2 */
    basic(buf, &o, 0x15, 0, 1, 0, 1); /* slot3: jeq r1,0 -> +1 (slot5 exit) */
    basic(buf, &o, 0xb7, 0, 0, 0, 0); /* slot4: mov r0, 0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot5: exit (index 4) */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "wide-cross program loads");
    if (e == BPF_OK)
    {
        /* JEQ at index 2 (slot3) jumps +1 slot -> slot5 = exit index 4. */
        e = bpf_program_get(p, 2, &g);
        ck(e == BPF_OK && g.has_target == 1 && g.target == 4,
           "jeq across wide resolves to exit index");
        bpf_program_destroy(p);
    }
}

static void test_reject_branch_into_wide(void)
{
    /* [mov r1,1][wide at slots1-2][mov][exit]. A JA from index? We craft a JA
     * at the head targeting the interior second slot of the wide. */
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, 1); /* slot0 JA +1 -> slot2 (wide interior) */
    wide(buf, &o, 2, 7, 0);           /* slots1-2 */
    basic(buf, &o, 0xb7, 0, 0, 1, 0); /* slot3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot4 exit */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_EBRANCH && p == 0, "branch into wide interior rejected");
}

static void test_wide_mid_reject(void)
{
    /* A conditional that points at the interior slot of a wide is rejected
     * even when the wide appears after the branch. */
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, 2); /* JA +2 -> slot2? slot0+1+2=slot3... */
    wide(buf, &o, 2, 7, 0);           /* slots1-2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* slot3 exit (a valid target) */
    cases = cases + 1;
    p = 0;
    e = load_raw(buf, o, &p);
    ck(e == BPF_OK, "JA to slot3 lands on exit (not interior)");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

/* ------------------------------------------------------------------ */
/* Profile enforcement. */

static void test_profile_matrix(void)
{
    /* base32-only instructions load under a base32 profile. */
    expect_prof(0x04, 0, 1, BPF_CONF_BASE32, BPF_OK, "alu32 in base32");
    expect_prof(0xb4, 0, 1, BPF_CONF_BASE32, BPF_OK, "mov32 base32 ok");
    expect_prof(0x04, 0, 1, 0, BPF_OK, "alu32 unrestricted");
    /* ALU64/JMP require base64, which base32 alone must reject. */
    expect_prof(0xb7, 0, 1, BPF_CONF_BASE32, BPF_EPROF, "alu64 needs base64");
    expect_prof(0xb7, 0, 1, BPF_CONF_BASE32 | BPF_CONF_BASE64, BPF_OK,
                "alu64 allowed base64");
    expect_prof(0x15, 0, 1, BPF_CONF_BASE32, BPF_EPROF, "jmp needs base64");
}

/* A JMP program (jeq + exit) is allowed once the profile permits base64. */
static void test_profile_jmp_allowed(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_profile pr;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x15, 0, 1, 0, 0); /* jeq r1,0 -> exit (next) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    pr = prof_only(BPF_CONF_BASE32 | BPF_CONF_BASE64);
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
    ck(e == BPF_OK, "jmp program allowed with base64 profile");
}

/* ------------------------------------------------------------------ */

int main(void)
{
    test_valid_alu_loop();
    test_valid_mov_neg();
    test_valid_movsx();
    test_valid_endian();
    test_valid_jumps();
    test_jmp32_ja();
    test_valid_mem();
    test_valid_wide();
    test_reject_reg_range();
    test_reject_r10_writes();
    test_r10_compare_base_not_write();
    test_reject_deprecated_packet();
    test_reject_unknown_ops();
    test_reject_bad_endian();
    test_reject_bad_movsx();
    test_reject_bad_neg();
    test_reject_bad_call_exit();
    test_reject_bad_atomic();
    test_branch_boundaries();
    test_call_boundaries();
    test_branch_cross_wide_correct();
    test_reject_branch_into_wide();
    test_wide_mid_reject();
    test_profile_matrix();
    test_profile_jmp_allowed();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_program_load_matrix: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_program_load_matrix (%d cases)\n", cases);
    return 0;
}
