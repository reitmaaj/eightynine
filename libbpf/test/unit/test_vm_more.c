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

static void test_alu_register_ops(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 40, 0); /* r1 = 40 */
    basic(buf, &o, 0xb7, 0, 2, 2, 0);  /* r2 = 2 */
    basic(buf, &o, 0xbf, 1, 0, 0, 0);  /* r0 = r1 */
    basic(buf, &o, 0x0f, 2, 0, 0, 0);  /* r0 += r2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "add register to r0");
}

static void test_alu_divmod(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0);
    basic(buf, &o, 0x37, 0, 0, 5, 0); /* r0 /= 5 = 8 */
    basic(buf, &o, 0x97, 0, 0, 3, 0); /* r0 %= 3 = 2 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 2, "divide then modulo");
}

static void test_sdiv_neg(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, -10, 0);
    basic(buf, &o, 0x37, 0, 0, 3, 1); /* signed div by 3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, (bpf_u64)0 - 3, "signed divide of -10 by 3");
}

static void test_register_compare(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);
    basic(buf, &o, 0xb7, 0, 1, 3, 0);
    basic(buf, &o, 0xb7, 0, 2, 5, 0);
    basic(buf, &o, 0x2d, 1, 1, 0, 1); /* jsgt? jgt r1,r2 -> false (3<5) */
    basic(buf, &o, 0xb7, 0, 0, 100, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 100, "unsigned register compare not taken");
}

static void test_jmp32_condition(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);
    basic(buf, &o, 0xb7, 0, 1, 0x100, 0); /* r1 = 256 */
    basic(buf, &o, 0x16, 0, 1, 0x100, 1); /* jeq32 r1,256 -> skip */
    basic(buf, &o, 0xb7, 0, 0, 100, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0, "jmp32 jeq taken (low 32 bits equal)");
}

static void test_memory_negative_offset(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100001, 0); /* base 0x100001 */
    basic(buf, &o, 0xb7, 0, 2, 77, 0);
    basic(buf, &o, 0x73, 2, 1, -1, 0); /* stx b *(r1-1) = 77 */
    basic(buf, &o, 0x71, 1, 0, -1, 0); /* ldx b r0 = *(r1-1) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 77, "negative offset store/load within region");
}

static void test_trapped_stays_trapped(void)
{
    bpf_byte buf[64];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x999999, 0);
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0x63, 2, 1, 0, 0); /* store to unmapped -> trap */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "trap program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 100, &vm);
    ck(e == BPF_OK, "create for trap persistence");
    if (e != BPF_OK)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = bpf_vm_step(vm);
    while (st == BPF_VM_RUNNING)
    {
        st = bpf_vm_step(vm);
    }
    ck(st == BPF_VM_TRAPPED, "trapped state reached");
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_TRAPPED, "trapped stays trapped across further steps");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_exhausted_stays(void)
{
    bpf_byte buf[32];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, -1);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "exhausted program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 1, &vm);
    ck(e == BPF_OK, "create with budget 1");
    if (e != BPF_OK)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_RUNNING, "first step consumes budget");
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_EXHAUSTED, "second step exhausts");
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_EXHAUSTED, "exhausted stays exhausted");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_context_unclobbered(void)
{
    bpf_byte buf[64];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "unclobbered program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 10, &vm);
    ck(e == BPF_OK, "create");
    if (e == BPF_OK)
    {
        (void)bpf_vm_step(vm);
        ck(bpf_vm_reg(vm, 1) == 0x1000 && bpf_vm_reg(vm, 2) == 0x40,
           "r1/r2 context unchanged across run");
        bpf_vm_destroy(vm);
    }
    bpf_program_destroy(prog);
}

static void test_memory_region_stays_zero(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0x7b, 2, 1, 0, 0); /* store dw 1 */
    basic(buf, &o, 0xb7, 0, 3, 0x100000 + 8, 0);
    basic(buf, &o, 0x79, 3, 0, 0, 0); /* read 8 bytes later -> 0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0, "bytes beyond a stored word remain zero");
}

static void test_count_loop(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);   /* r0 = 0 */
    basic(buf, &o, 0xb7, 0, 1, 100, 0); /* r1 = 100 */
    basic(buf, &o, 0x07, 0, 0, 1, 0);   /* r0 += 1 */
    basic(buf, &o, 0x17, 0, 1, 1, 0);   /* r1 -= 1 */
    basic(buf, &o, 0x15, 0, 1, 0, 1);   /* if r1==0 -> exit */
    basic(buf, &o, 0x05, 0, 0, 0, -4);  /* loop back to r0+=1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 100, "count loop runs 100 times");
}

static void test_store_immediate_form(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0x62, 0, 1, 42, 0); /* st w [r1+0] = 42 (immediate) */
    basic(buf, &o, 0x61, 1, 0, 0, 0);  /* ldx w r0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "store immediate then load");
}

static void test_store_imm_offset(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0x7a, 0, 1, 9, 8); /* st dw [r1+8] = 9 */
    basic(buf, &o, 0x79, 1, 0, 0, 8); /* ldx dw r0 = [r1+8] */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 9, "store immediate with offset");
}

static void test_alu_neg(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0);
    basic(buf, &o, 0x87, 0, 0, 0, 0); /* r0 = -r0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, (bpf_u64)0 - 42, "negate register");
}

static void test_alu64_xor_self(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x1234, 0);
    basic(buf, &o, 0xaf, 1, 1, 0, 0); /* r1 ^= r1 -> 0 */
    basic(buf, &o, 0xbf, 1, 0, 0, 0); /* r0 = r1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0, "xor register with itself clears it");
}

static void test_mem_word_offset_chain(void)
{
    bpf_byte buf[160];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, &o, 0xb7, 0, 2, 11, 0);
    basic(buf, &o, 0x63, 2, 1, 0, 0);  /* [r1+0] = 11 */
    basic(buf, &o, 0x07, 0, 2, 22, 0); /* r2 = 33 */
    basic(buf, &o, 0x63, 2, 1, 0, 4);  /* [r1+4] = 33 */
    basic(buf, &o, 0x61, 1, 0, 0, 0);  /* r0 = [r1+0] */
    basic(buf, &o, 0x0f, 1, 0, 0, 0);  /* r0 += r1 (base, unrelated) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0x10000b, "stored value plus base");
}

static void test_multi_reg_result(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 3, 7, 0); /* r3 = 7 */
    basic(buf, &o, 0xb7, 0, 4, 6, 0); /* r4 = 6 */
    basic(buf, &o, 0x2f, 4, 3, 0, 0); /* r3 *= r4 = 42 */
    basic(buf, &o, 0xbf, 3, 0, 0, 0); /* r0 = r3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 42, "multiply registers");
}

static void test_mov_negative_imm(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, -1, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, (bpf_u64)0 - 1, "mov -1 sign extends to all ones");
}

static void test_add_wraps_high(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0x7FFFFFFF, 0);
    basic(buf, &o, 0x07, 0, 0, 1, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 0x80000000UL, "add wraps into the high word");
}

static void test_mod_small(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 100, 0);
    basic(buf, &o, 0x97, 0, 0, 30, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 10, "modulo");
}

static void test_r3_register_snapshot(void)
{
    bpf_byte buf[96];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 3, 0xABCD, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "snapshot program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 100, &vm);
    ck(e == BPF_OK, "create for snapshot");
    if (e == BPF_OK)
    {
        ck(bpf_vm_reg(vm, 3) == 0, "r3 zero before run");
        st = bpf_vm_step(vm);
        ck(st == BPF_VM_RUNNING && bpf_vm_reg(vm, 3) == 0xABCD,
           "r3 updated and snapshot reads it");
        bpf_vm_destroy(vm);
    }
    bpf_program_destroy(prog);
}

static void test_r6_r9_usable(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 6, 5, 0);
    basic(buf, &o, 0xb7, 0, 9, 9, 0);
    basic(buf, &o, 0xbf, 6, 0, 0, 0);
    basic(buf, &o, 0x0f, 9, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 14, "callee-saved registers usable");
}

int main(void)
{
    test_alu_register_ops();
    test_alu_divmod();
    test_sdiv_neg();
    test_register_compare();
    test_jmp32_condition();
    test_memory_negative_offset();
    test_trapped_stays_trapped();
    test_exhausted_stays();
    test_context_unclobbered();
    test_memory_region_stays_zero();
    test_count_loop();
    test_store_immediate_form();
    test_store_imm_offset();
    test_alu_neg();
    test_alu64_xor_self();
    test_mem_word_offset_chain();
    test_multi_reg_result();
    test_mov_negative_imm();
    test_add_wraps_high();
    test_mod_small();
    test_r3_register_snapshot();
    test_r6_r9_usable();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_more: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm_more (%d cases)\n", cases);
    return 0;
}
