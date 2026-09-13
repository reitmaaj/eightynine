#include <stdio.h>

#include "vm.h"

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

static bpf_program *load_prog(bpf_byte *buf, bpf_u32 len)
{
    bpf_profile pr;
    bpf_program *p;
    bpf_err e;

    pr.allowed = 0;
    pr.max_insn = 0;
    p = 0;
    e = bpf_program_load(buf, len, &pr, &p);
    if (e != BPF_OK)
    {
        return 0;
    }
    return p;
}

static bpf_region_cfg mk(bpf_byte kind, bpf_off64 base, bpf_off64 len)
{
    bpf_region_cfg c;

    c.kind = kind;
    c.guest = base;
    c.len = len;
    c.init = 0;
    c.init_len = 0;
    return c;
}

/* Regions: input [0x1000], working [0x100000], stack [0xF0000]. */
static void regs3(bpf_region_cfg *c)
{
    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x100000, 0x2000);
    c[2] = mk(BPF_RSTACK, 0xF0000, 0x2000);
}

static bpf_vm_state run_prog(bpf_program *prog, bpf_u64 budget, bpf_u64 *r0)
{
    bpf_region_cfg c[3];
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    regs3(c);
    e = bpf_vm_create(prog, c, 3, budget, &vm);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    st = BPF_VM_RUNNING;
    while (st == BPF_VM_RUNNING)
    {
        st = bpf_vm_step(vm);
    }
    *r0 = bpf_vm_reg(vm, 0);
    bpf_vm_destroy(vm);
    return st;
}

/* Expect a program to return `want` with state RETURNED. */
static void expect_r0(bpf_byte *buf, bpf_u32 len, bpf_u64 want,
                      const char *name)
{
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;

    cases = cases + 1;
    prog = load_prog(buf, len);
    if (prog == 0)
    {
        ck(0, name);
        return;
    }
    r0 = 0;
    st = run_prog(prog, 1000, &r0);
    ck(st == BPF_VM_RETURNED && r0 == want, name);
    bpf_program_destroy(prog);
}

/* ------------------------------------------------------------------ */
/* ALU and immediate semantics. */

static void test_alu_arithmetic(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 5, 0); /* r0 = 5 */
    basic(buf, &o, 0x07, 0, 0, 3, 0); /* r0 += 3 */
    basic(buf, &o, 0x17, 0, 0, 1, 0); /* r0 -= 1 */
    basic(buf, &o, 0x37, 0, 0, 4, 0); /* r0 /= 4 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 1, "alu add/sub/div");
}

static void test_alu_multiply(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 6, 0);
    basic(buf, &o, 0x27, 0, 0, 7, 0); /* r0 *= 7 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "alu multiply 6*7");
}

static void test_alu_and_or_xor(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0x0F, 0); /* r0 = 0x0F */
    basic(buf, &o, 0x57, 0, 0, 0x03, 0); /* r0 &= 0x03 */
    basic(buf, &o, 0x47, 0, 0, 0x40, 0); /* r0 |= 0x40 */
    basic(buf, &o, 0xa7, 0, 0, 0x55, 0); /* r0 ^= 0x55 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0x16, "alu and/or/xor");
}

static void test_alu_shift(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);
    basic(buf, &o, 0x67, 0, 0, 5, 0); /* r0 <<= 5 */
    basic(buf, &o, 0x77, 0, 0, 2, 0); /* r0 >>= 2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 8, "alu lsh/rsh");
}

static void test_alu_32_truncation(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, -1, 0); /* r0 = -1 (64-bit) */
    basic(buf, &o, 0x04, 0, 0, 1, 0);  /* r0 = (u32)r0 + 1 -> 0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0, "alu32 wraps destination to 32 bits");
}

static void test_wide_immediate_64(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    wide(buf, &o, 0, 0x89ABCDEFu, 0x01234567u);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0x0123456789ABCDEFUL, "lddw loads full 64-bit immediate");
}

static void test_alu64_mov_register(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 99, 0); /* r1 = 99 */
    basic(buf, &o, 0xbf, 1, 0, 0, 0);  /* r0 = r1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 99, "mov register");
}

/* ------------------------------------------------------------------ */
/* Control flow. */

static void test_unconditional_branch_skips(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);
    basic(buf, &o, 0x05, 0, 0, 0, 2);  /* ja +2 */
    basic(buf, &o, 0xb7, 0, 0, 7, 0);  /* skipped */
    basic(buf, &o, 0xb7, 0, 0, 8, 0);  /* skipped */
    basic(buf, &o, 0x07, 0, 0, 41, 0); /* r0 += 41 -> 42 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "unconditional branch skips two instructions");
}

static void test_conditional_taken(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 10, 0);
    basic(buf, &o, 0x15, 0, 1, 10, 1); /* jeq r1,10 -> skip next */
    basic(buf, &o, 0xb7, 0, 0, 1, 0);  /* skipped */
    basic(buf, &o, 0xb7, 0, 0, 2, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 2, "jeq taken skips");
}

static void test_conditional_not_taken(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);
    basic(buf, &o, 0xb7, 0, 1, 5, 0);
    basic(buf, &o, 0x15, 0, 1, 10, 1); /* jeq r1,10 false -> fall through */
    basic(buf, &o, 0xb7, 0, 0, 100, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 100, "jeq not taken falls through");
}

static void test_signed_less(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);
    basic(buf, &o, 0xb7, 0, 1, -5, 0);
    basic(buf, &o, 0x65, 0, 1, 0, 1);   /* jsgt r1,0 signed(-5) false */
    basic(buf, &o, 0xb7, 0, 0, 100, 0); /* fall-through */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 100, "jsgt signed comparison not taken for -5");
}

static void test_budget_exhausted_loop(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, -1); /* infinite self-loop */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "loop loads");
    if (prog == 0)
    {
        return;
    }
    r0 = 0;
    st = run_prog(prog, 50, &r0);
    ck(st == BPF_VM_EXHAUSTED, "infinite loop hits budget");
    bpf_program_destroy(prog);
}

/* ------------------------------------------------------------------ */
/* Local calls and EXIT. */

static void test_local_call(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    /* fn: r0 = 42; exit.  call fn; exit. */
    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);  /* idx0 r0=0 */
    basic(buf, &o, 0x85, 1, 0, 1, 0);  /* idx1 call imm1 -> idx2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx2 exit (return point) */
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* idx3 r0=42 (skipped) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx4 exit */
    expect_r0(buf, o, 42, "local call returns into function");
}

static void test_nested_call_returns(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    /* inner: r1 += 5; exit. outer: call inner; r0 = r1; exit. */
    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 37, 0); /* idx0 r1=37 */
    basic(buf, &o, 0x85, 1, 0, 2, 0);  /* idx1 call imm2 -> idx3 */
    basic(buf, &o, 0xbf, 1, 0, 0, 0);  /* idx2 r0 = r1 (return point) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx3 exit */
    basic(buf, &o, 0x07, 0, 1, 5, 0);  /* idx4 inner: r1 += 5 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx5 exit -> returns to idx2 */
    expect_r0(buf, o, 42, "call returns to instruction after call");
}

/* ------------------------------------------------------------------ */
/* Memory. */

static void test_mem_store_load(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0); /* r1 = working base */
    basic(buf, &o, 0xb7, 0, 2, 42, 0);       /* r2 = 42 */
    basic(buf, &o, 0x63, 2, 1, 0, 0);        /* stx w *(r1)=r2 */
    basic(buf, &o, 0x61, 1, 0, 0, 0);        /* ldx w r0 = *(r1) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "store then load word");
}

static void test_mem_byte_stores(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x04030201, 0);
    basic(buf, &o, 0x7b, 2, 1, 0, 0); /* stx dw r1[0]=r2 */
    basic(buf, &o, 0x71, 1, 0, 0, 0); /* ldx b r0=byte0 -> 0x01 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 1, "load byte reads low byte");
}

static void test_mem_halfword(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x04030201, 0);
    basic(buf, &o, 0x7b, 2, 1, 0, 0);
    basic(buf, &o, 0x69, 1, 0, 0, 0); /* ldx h r0 = 0x0201 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0x0201, "load halfword reads low 16 bits");
}

static void test_mem_sign_extend_load(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x80, 0); /* 0x80 as a signed byte = -128 */
    basic(buf, &o, 0x73, 2, 1, 0, 0);    /* stx b mem[0] = 0x80 */
    basic(buf, &o, 0x91, 1, 0, 0, 0);    /* ldxsxb b sign-extend -> -128 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, (bpf_u64)0 - 128, "sign-extending byte load");
}

static void test_mem_out_of_bounds_traps(void)
{
    bpf_byte buf[64];
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x300000, 0); /* unmapped */
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0x63, 2, 1, 0, 0); /* store to unmapped */
    basic(buf, &o, 0xb7, 0, 0, 7, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "oob program loads");
    if (prog == 0)
    {
        return;
    }
    r0 = 0;
    st = run_prog(prog, 100, &r0);
    ck(st == BPF_VM_TRAPPED, "store to unmapped address traps");
    bpf_program_destroy(prog);
}

static void test_stack_read_as_base(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 7, 0);  /* r1 = 7 */
    basic(buf, &o, 0x73, 1, 10, 0, 0); /* stx b *(r10 + 0) = 7 */
    basic(buf, &o, 0x71, 10, 0, 0, 0); /* ldx b r0 = *(r10 + 0) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 7, "read/write through r10 frame pointer");
}

int main(void)
{
    test_alu_arithmetic();
    test_alu_multiply();
    test_alu_and_or_xor();
    test_alu_shift();
    test_alu_32_truncation();
    test_wide_immediate_64();
    test_alu64_mov_register();
    test_unconditional_branch_skips();
    test_conditional_taken();
    test_conditional_not_taken();
    test_signed_less();
    test_budget_exhausted_loop();
    test_local_call();
    test_nested_call_returns();
    test_mem_store_load();
    test_mem_byte_stores();
    test_mem_halfword();
    test_mem_sign_extend_load();
    test_mem_out_of_bounds_traps();
    test_stack_read_as_base();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm (%d cases)\n", cases);
    return 0;
}
