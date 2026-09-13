#include <stdio.h>

#include "eval.h"

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

static bpf_status run(const bpf_insn *prog, bpf_u32 n, bpf_u64 *r0)
{
    bpf_machine m;
    bpf_status st;

    bpf_machine_init(&m, prog, n);
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    *r0 = m.regs.r[0];
    return st;
}

static void test_returned(void)
{
    bpf_insn prog[2];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 42, 0);
    prog[1] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 2, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 42, "clean exit returns 42");
}

static void test_fell_off_end(void)
{
    bpf_insn prog[2];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0); /* no exit; falls past prog[1] */
    prog[1] = mk(0xb7, 0, 1, 2, 0);
    st = run(prog, 2, &r0);
    ck(st == BPF_STAT_ERR, "no exit falls off -> ERR");
}

static void test_jump_out_of_bounds(void)
{
    bpf_insn prog[2];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0x05, 0, 0, 0, 99); /* JA to pc 100, out of bounds */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 2, &r0);
    ck(st == BPF_STAT_ERR, "jump out of bounds -> ERR");
}

static void test_budget_exhausted(void)
{
    bpf_insn prog[1];
    bpf_status st;

    prog[0] = mk(0x05, 0, 0, 0, -1); /* infinite self-jump */
    {
        bpf_machine m;

        bpf_machine_init(&m, prog, 1);
        m.budget = 8;
        st = BPF_STAT_RUNNING;
        while (st == BPF_STAT_RUNNING)
        {
            st = bpf_step(&m);
        }
    }
    ck(st == BPF_STAT_EXHAUSTED, "infinite loop exhausted by budget");
}

static void test_trap_memory(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[4];

    prog[0] = mk(0xb7, 0, 1, 100, 0); /* r1 = 100 (OOB base) */
    prog[1] = mk(0x71, 1, 0, 0, 0);   /* LDX B r0 = mem[100] -> trap */
    bpf_machine_init(&m, prog, 2);
    m.mem = mem;
    m.mem_size = 4;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    ck(st == BPF_STAT_TRAP, "oob load -> TRAP");
}

static void test_hostcall_helper(void)
{
    bpf_insn prog[2];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0x85, 0, 0, 3, 0); /* CALL helper 3 */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 2, &r0);
    ck(st == BPF_STAT_HOSTCALL, "helper call -> HOSTCALL");
}

static void test_stack_overflow_terminal(void)
{
    bpf_insn prog[2];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0x85, 1, 0, 0, 0);  /* CALL local -> pc1 */
    prog[1] = mk(0x85, 1, 0, -1, 0); /* CALL local -> pc0 */
    st = run(prog, 2, &r0);
    ck(st == BPF_STAT_ERR, "deep recursion -> ERR");
}

int main(void)
{
    test_returned();
    test_fell_off_end();
    test_jump_out_of_bounds();
    test_budget_exhausted();
    test_trap_memory();
    test_hostcall_helper();
    test_stack_overflow_terminal();
    if (fails)
    {
        (void)fprintf(stderr, "test_run_status: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_run_status\n");
    return 0;
}
