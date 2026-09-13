#include <stdio.h>

#include "program.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

/* Minimal raw-bytecode builder. */

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

/* Wide (IMM LD) instruction; second slot reserved fields are zero. */
static void wide(bpf_byte *buf, bpf_u32 *o, bpf_u32 dst, bpf_u32 lo, bpf_u32 hi)
{
    basic(buf, o, 0x18u, 0, dst, (int)lo, 0);
    e8(buf, o, 0);
    e8(buf, o, 0);
    e16(buf, o, 0);
    e32(buf, o, hi);
}

static bpf_profile unrestricted(void)
{
    bpf_profile pr;

    pr.allowed = 0;
    pr.max_insn = 0;
    return pr;
}

static bpf_profile only(bpf_u32 allowed)
{
    bpf_profile pr;

    pr.allowed = allowed;
    pr.max_insn = 0;
    return pr;
}

static bpf_err load(bpf_byte *buf, bpf_u32 len, bpf_program **out)
{
    bpf_profile pr;

    pr = unrestricted();
    return bpf_program_load(buf, len, &pr, out);
}

static void test_empty(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;

    p = 0;
    e = load(buf, 0, &p);
    ck(e == BPF_EEMPTY && p == 0, "empty buffer rejected");
}

static void test_not_multiple_of_8(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;

    p = 0;
    e = load(buf, 4, &p);
    ck(e == BPF_ETRUNC && p == 0, "length not a multiple of 8 rejected");
}

static void test_truncated_wide(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;

    buf[0] = 0x18;
    p = 0;
    e = load(buf, 8, &p);
    ck(e == BPF_ETRUNC && p == 0, "wide cut short rejected");
}

static void test_simple_load(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* mov r0, 42 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* exit */
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_OK && p != 0, "simple program loads");
    if (e == BPF_OK)
    {
        ck(bpf_program_count(p) == 2, "count is 2");
        ck((bpf_program_conformance(p) & BPF_CONF_BASE64) != 0u,
           "conf includes base64");
        bpf_program_destroy(p);
    }
}

static void test_unknown_alu(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xE7, 0, 0, 0, 0); /* ALU64 reserved op code 0xE */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_EUNSUP && p == 0, "reserved ALU op rejected");
}

static void test_unknown_jmp(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xE5, 0, 0, 0, 0); /* JMP reserved op code 0xE */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_EUNSUP && p == 0, "reserved JMP op rejected");
}

static void test_wide_reserved(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    wide(buf, &o, 0, 42, 0);
    buf[8] = 0x01; /* corrupt reserved continuation opcode */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_ERES && p == 0, "non-zero wide continuation rejected");
}

static void test_write_r10(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x07, 0, 10, 1, 0); /* ALU64 ADD r10, 1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_ER10 && p == 0, "ALU64 write to r10 rejected");
}

static void test_read_r10_base_ok(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x61, 10, 0, 0, 0); /* LDX W r0 = *(u32 *)(r10 + 0) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_OK, "r10 as base register is accepted");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

static void test_atomic_fetch_write_r10(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x61, 10, 1, 0, 0); /* preload base r1 = *(r10) */
    basic(buf, &o, 0xC3, 10, 1, 1, 0); /* atomic fetch ADD, src r10 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_ER10 && p == 0, "atomic fetch write to r10 rejected");
}

static void test_branch_into_wide_rejected(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, 1); /* JA +1: target slot 2 (mid-wide) */
    wide(buf, &o, 0, 42, 0);          /* slots 1,2 */
    basic(buf, &o, 0xb7, 0, 0, 1, 0); /* mov r0, 1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* exit */
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_EBRANCH && p == 0, "branch into wide continuation rejected");
}

static void test_branch_past_end_rejected(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, 99); /* JA +99 off end */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_EBRANCH && p == 0, "branch past end rejected");
}

static void test_backward_branch_ok(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);  /* mov r0, 1 */
    basic(buf, &o, 0x05, 0, 0, 0, -1); /* JA -1: self loop (backward) */
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_OK, "backward branch accepted");
    if (e == BPF_OK)
    {
        bpf_program_destroy(p);
    }
}

static void test_wide_cross_branch_target(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, 2); /* JA +2 crosses the wide below */
    wide(buf, &o, 0, 42, 0);          /* wide: slots 1,2 */
    basic(buf, &o, 0xb7, 0, 0, 0, 0); /* mov r0, 0 : slot 3, index 2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* exit : slot 4, index 3 */
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_OK, "wide-crossing program loads");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 0, &g);
        ck(e == BPF_OK && g.has_target == 1, "JA has a resolved target");
        /* Slot-correct target is index 2 (the mov); naive compressed index
         * arithmetic would produce index 3 (the exit). */
        ck(g.target == 2, "JA resolves across a wide to slot-correct index");
        bpf_program_destroy(p);
    }
}

static void test_profile_reject(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_profile pr;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* ALU64 mov -> needs base64 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    pr = only(BPF_CONF_BASE32);
    p = 0;
    e = bpf_program_load(buf, o, &pr, &p);
    ck(e == BPF_EPROF && p == 0, "base64 program rejected by base32 profile");
}

static void test_get_out_of_range(void)
{
    bpf_byte buf[64] = {0};
    bpf_program *p;
    bpf_err e;
    bpf_u32 o;
    bpf_prog_insn g;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    p = 0;
    e = load(buf, o, &p);
    ck(e == BPF_OK, "load for get test");
    if (e == BPF_OK)
    {
        e = bpf_program_get(p, 99, &g);
        ck(e == BPF_EREG, "get out of range returns EREG");
        bpf_program_destroy(p);
    }
}

int main(void)
{
    test_empty();
    test_not_multiple_of_8();
    test_truncated_wide();
    test_simple_load();
    test_unknown_alu();
    test_unknown_jmp();
    test_wide_reserved();
    test_write_r10();
    test_read_r10_base_ok();
    test_atomic_fetch_write_r10();
    test_branch_into_wide_rejected();
    test_branch_past_end_rejected();
    test_backward_branch_ok();
    test_wide_cross_branch_target();
    test_profile_reject();
    test_get_out_of_range();
    if (fails)
    {
        (void)fprintf(stderr, "test_program_load: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program_load\n");
    return 0;
}
