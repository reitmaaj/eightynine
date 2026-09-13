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

static void test_init(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    int i;

    prog[0] = mk(0x95, 0, 0, 0, 0);
    bpf_machine_init(&m, prog, 1);
    ck(m.pc == 0, "init pc 0");
    ck(m.n == 1, "init n 1");
    ck(m.stack_n == 0, "init empty stack");
    ck(m.budget == 100000, "init budget");
    ck(m.halted == 0, "init not halted");
    ck(m.mem == 0 && m.mem_size == 0, "init no memory");
    ck(m.helper == 0, "init helper 0");
    for (i = 0; i < BPF_NREG; ++i)
    {
        ck(m.regs.r[i] == 0, "init regs zero");
    }
}

static void test_halted_returns(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);
    bpf_machine_init(&m, prog, 1);
    m.halted = 1;
    st = bpf_step(&m);
    ck(st == BPF_STAT_RETURNED, "halted -> returned");
}

static void test_budget_decrement(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);
    prog[1] = mk(0x05, 0, 0, 0, -1); /* self-jump (no exit) */
    bpf_machine_init(&m, prog, 2);
    m.budget = 2;
    st = bpf_step(&m);
    ck(st == BPF_STAT_RUNNING && m.budget == 1, "budget decremented");
    st = bpf_step(&m);
    ck(st == BPF_STAT_RUNNING && m.budget == 0, "budget 0 after 2nd");
    st = bpf_step(&m);
    ck(st == BPF_STAT_EXHAUSTED, "budget exhausted on 3rd");
}

static void test_forward_backward_jumps(void)
{
    bpf_insn prog[4];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0x05, 0, 0, 0, 2); /* ja -> 3 */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    prog[2] = mk(0x95, 0, 0, 0, 0);
    prog[3] = mk(0x95, 0, 0, 0, 0);
    bpf_machine_init(&m, prog, 4);
    st = bpf_step(&m);
    ck(st == BPF_STAT_RUNNING && m.pc == 3, "forward ja");

    prog[0] = mk(0xb7, 0, 0, 1, 0);
    prog[1] = mk(0x05, 0, 0, 0, -1); /* ja -> 1 (self) */
    bpf_machine_init(&m, prog, 2);
    m.budget = 5;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    ck(st == BPF_STAT_EXHAUSTED, "backward self-jump exhausted");
}

static void test_call_stack_boundaries(void)
{
    bpf_insn prog[4];
    bpf_machine m;
    bpf_status st;
    int i;

    prog[0] = mk(0x85, 1, 0, 1, 0); /* CALL -> 2 */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    prog[2] = mk(0x85, 1, 0, 0, 0);  /* CALL -> 3 */
    prog[3] = mk(0x85, 1, 0, -1, 0); /* CALL -> 3 (self) */
    bpf_machine_init(&m, prog, 4);
    st = bpf_step(&m); /* CALL -> 2, push 1 */
    ck(st == BPF_STAT_RUNNING && m.stack_n == 1 && m.pc == 2, "call push");
    st = bpf_step(&m); /* CALL -> 3, push 3 */
    ck(st == BPF_STAT_RUNNING && m.stack_n == 2 && m.pc == 3, "nested push");
    /* Now loop at pc3 pushing until overflow. */
    for (i = 0; i < BPF_STACK_N; ++i)
    {
        st = bpf_step(&m);
        if (st != BPF_STAT_RUNNING)
        {
            break;
        }
    }
    ck(st == BPF_STAT_ERR, "stack overflow -> err");
}

static void test_invalid_class(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0x95, 0, 0, 0, 0);
    prog[0].class = 0x80; /* out-of-range class byte */
    bpf_machine_init(&m, prog, 1);
    st = bpf_step(&m);
    ck(st == BPF_STAT_ERR, "invalid class -> err");
}

int main(void)
{
    test_init();
    test_halted_returns();
    test_budget_decrement();
    test_forward_backward_jumps();
    test_call_stack_boundaries();
    test_invalid_class();
    if (fails)
    {
        (void)fprintf(stderr, "test_machine: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_machine\n");
    return 0;
}
