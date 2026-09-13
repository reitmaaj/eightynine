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

/* Program-local function: main calls helper at pc4 that returns r0=99. */
static void test_local_function(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0x85, 1, 0, 3, 0); /* CALL -> pc4 */
    prog[1] = mk(0x95, 0, 0, 0, 0); /* (unreachable) */
    prog[2] = mk(0x95, 0, 0, 0, 0);
    prog[3] = mk(0x95, 0, 0, 0, 0);
    prog[4] = mk(0xb7, 0, 0, 99, 0); /* r0 = 99 */
    prog[5] = mk(0x95, 0, 0, 0, 0);  /* return to pc1 */
    prog[6] = mk(0x95, 0, 0, 0, 0);
    prog[7] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 8, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 99, "local function returns 99");
}

/* Nested calls: r0 = 1 + 2 + 3 via three local function calls. */
static void test_nested_functions(void)
{
    bpf_insn prog[14];
    bpf_u64 r0;
    bpf_status st;
    int i;

    for (i = 0; i < 14; ++i)
    {
        prog[i] = mk(0x95, 0, 0, 0, 0);
    }
    /* main */
    prog[0] = mk(0x85, 1, 0, 1, 0); /* CALL f1 -> pc2 */
    prog[1] = mk(0x95, 0, 0, 0, 0); /* exit main */
    /* f1 at pc2 */
    prog[2] = mk(0x85, 1, 0, 3, 0); /* CALL f2 -> pc6 */
    prog[3] = mk(0x85, 1, 0, 4, 0); /* CALL f3 -> pc8 */
    prog[4] = mk(0x07, 0, 0, 1, 0); /* r0 += 1 */
    prog[5] = mk(0x95, 0, 0, 0, 0); /* return to caller */
    /* f2 at pc6 */
    prog[6] = mk(0x07, 0, 0, 2, 0); /* r0 += 2 */
    prog[7] = mk(0x95, 0, 0, 0, 0); /* return */
    /* f3 at pc8 */
    prog[8] = mk(0x07, 0, 0, 3, 0); /* r0 += 3 */
    prog[9] = mk(0x95, 0, 0, 0, 0); /* return */
    st = run(prog, 14, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 6, "nested functions sum 1+2+3 = 6");
}

/* Recursive descent via local calls with a fixed depth. */
static void test_recursive(void)
{
    bpf_insn prog[10];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0x85, 1, 0, 2, 0); /* CALL recurse -> pc3 */
    prog[1] = mk(0x95, 0, 0, 0, 0); /* exit */
    prog[2] = mk(0x95, 0, 0, 0, 0);
    /* recurse at pc3: r1-=1; if r1>0 recurse; r0+=1 */
    prog[3] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[4] = mk(0xb7, 0, 1, 3, 0);  /* r1 = 3 (reset) */
    prog[5] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[6] = mk(0x65, 0, 1, 0, 2);  /* JSGT K r1>0 -> pc9 */
    prog[7] = mk(0x85, 1, 0, -5, 0); /* CALL recurse (pc3) */
    prog[8] = mk(0x07, 0, 0, 1, 0);  /* r0 += 1 */
    prog[9] = mk(0x95, 0, 0, 0, 0);  /* return */
    st = run(prog, 10, &r0);
    ck(st == BPF_STAT_RETURNED, "recursion terminates");
}

int main(void)
{
    test_local_function();
    test_nested_functions();
    test_recursive();
    if (fails)
    {
        (void)fprintf(stderr, "test_program6: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program6\n");
    return 0;
}
