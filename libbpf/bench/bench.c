#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <time.h>

#include "eval.h"
#include "progs.h"

#define MAX_PROG 256
#define MICRO_COPIES 64
#define MICRO_MEM_SIZE (MICRO_COPIES * 8UL + 16UL)
#define MAX_CASES 32

typedef struct bench_case
{
    const char *name;
    bpf_insn *prog;
    bpf_u32 n;
    bpf_byte *mem;
    bpf_u64 mem_size;
    int verify_copy;
} bench_case;

static int fails;

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

/* Run a program to completion, returning the number of steps taken. */
static bpf_u64 run_once(bpf_insn *prog, bpf_u32 n, bpf_byte *mem,
                        bpf_u64 mem_size, bpf_status *out)
{
    bpf_machine m;
    bpf_status st;
    bpf_u64 steps;

    bpf_machine_init(&m, prog, n);
    m.mem = mem;
    m.mem_size = mem_size;
    steps = 0;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
        steps = steps + 1;
    }
    *out = st;
    return steps;
}

static void stamp(struct timespec *t)
{
    (void)clock_gettime(CLOCK_MONOTONIC, t);
}

static double nsec(const struct timespec *a, const struct timespec *b)
{
    long s;
    long n;

    s = (long)(b->tv_sec - a->tv_sec);
    n = b->tv_nsec - a->tv_nsec;
    return (double)s * 1e9 + (double)n;
}

/* Verify correctness of a case before timing it. Returns 1 on success. */
static int verify(bench_case *c)
{
    bpf_u64 steps;
    bpf_status st;
    int i;

    steps = run_once(c->prog, c->n, c->mem, c->mem_size, &st);
    if (st != BPF_STAT_RETURNED)
    {
        (void)fprintf(stderr, "FAIL: %s did not return (status %d)\n", c->name,
                      (int)st);
        return 0;
    }
    if (steps == 0)
    {
        (void)fprintf(stderr, "FAIL: %s executed zero steps\n", c->name);
        return 0;
    }
    if (c->verify_copy)
    {
        for (i = 0; i < 1000; ++i)
        {
            if (c->mem[1000 + i] != c->mem[i])
            {
                (void)fprintf(stderr, "FAIL: %s copy mismatch at %d\n",
                              c->name, i);
                return 0;
            }
        }
    }
    return 1;
}

/* Build a straight-line micro case: `copies` copies of `op`. ALU/JMP ops use
 * a rotating destination register; memory ops use r9 as a base and rotate the
 * destination register over ascending offsets. Returns the instruction count. */
static int build_micro(bpf_insn *out, const char *name, bpf_byte op,
                       int is_mem, bpf_byte *mem, bench_case *c)
{
    int i;
    int dst;

    c->name = name;
    c->prog = out;
    c->mem = is_mem ? mem : 0;
    c->mem_size = is_mem ? MICRO_MEM_SIZE : 0UL;
    c->verify_copy = 0;
    out[0] = mk(0xb7, 0, 9, 0, 0); /* r9 = base address 0 */
    for (i = 0; i < MICRO_COPIES; ++i)
    {
        dst = (i % 8) + 1;
        if (is_mem)
        {
            /* STX reads the address from dst_reg and the value from src_reg;
             * other memory ops read the address from src_reg. */
            if (op == 0x63u)
            {
                out[1 + i] = mk(op, (bpf_byte)dst, 9, 0, i * 8);
            }
            else
            {
                out[1 + i] = mk(op, 9, (bpf_byte)dst, 0, i * 8);
            }
        }
        else
        {
            out[1 + i] = mk(op, 0, (bpf_byte)dst, (i * 7) + 1, 0);
        }
    }
    out[1 + MICRO_COPIES] = mk(0x95, 0, 0, 0, 0); /* EXIT */
    c->n = (bpf_u32)(MICRO_COPIES + 2);
    return (int)c->n;
}

static bpf_u64 pick_repeat(double run_ns)
{
    bpf_u64 r;

    r = 1;
    if (run_ns > 0.0)
    {
        r = (bpf_u64)(0.2e9 / run_ns);
    }
    if (r < 1)
    {
        r = 1;
    }
    if (r > 100000UL)
    {
        r = 100000UL;
    }
    return r;
}

static void measure(bench_case *c, bpf_u64 steps_per_run, double *ns_per_step,
                    double *mips)
{
    struct timespec a;
    struct timespec b;
    bpf_status st;
    bpf_u64 repeat;
    bpf_u64 i;
    double run_ns;
    double total_ns;
    double pstep;

    run_once(c->prog, c->n, c->mem, c->mem_size, &st); /* warmup */
    run_once(c->prog, c->n, c->mem, c->mem_size, &st);
    run_once(c->prog, c->n, c->mem, c->mem_size, &st);

    stamp(&a);
    run_once(c->prog, c->n, c->mem, c->mem_size, &st);
    stamp(&b);
    run_ns = nsec(&a, &b);
    repeat = pick_repeat(run_ns);

    stamp(&a);
    for (i = 0; i < repeat; ++i)
    {
        run_once(c->prog, c->n, c->mem, c->mem_size, &st);
    }
    stamp(&b);
    total_ns = nsec(&a, &b);
    pstep = total_ns / ((double)repeat * (double)steps_per_run);
    *ns_per_step = pstep;
    *mips = 1e9 / pstep / 1e6;
}

int main(void)
{
    bpf_insn progs[MAX_CASES][MAX_PROG];
    bpf_byte mems[MAX_CASES][16384];
    const bench_prog *bp;
    bench_case cases[MAX_CASES];
    int nc;
    int i;
    FILE *csv;
    bpf_u64 steps;
    bpf_status st;
    double nsps;
    double mips;

    nc = 0;
    for (bp = bench_programs; bp->name != 0; ++bp)
    {
        cases[nc].name = bp->name;
        cases[nc].prog = progs[nc];
        cases[nc].mem = mems[nc];
        cases[nc].mem_size = bp->mem_size;
        cases[nc].verify_copy = (bp->mem_size != 0UL);
        cases[nc].n = (bpf_u32)bp->build(progs[nc], MAX_PROG);
        ++nc;
    }

    build_micro(progs[nc], "micro:alu64-add", 0x07, 0, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:alu64-mul", 0x27, 0, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:alu64-div", 0x37, 0, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:alu64-mov", 0xb7, 0, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:jump-ja", 0x05, 0, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:ldx-w", 0x61, 1, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:stx-w", 0x63, 1, mems[nc], &cases[nc]); ++nc;
    build_micro(progs[nc], "micro:ldx-dw", 0x79, 1, mems[nc], &cases[nc]); ++nc;

    csv = fopen("bench/bench.csv", "w");
    if (csv == 0)
    {
        (void)fprintf(stderr, "FAIL: cannot open bench/bench.csv\n");
        return 1;
    }
    (void)fprintf(csv, "case,steps_per_run,ns_per_step,mips\n");

    (void)printf("%-18s %8s %12s %10s\n", "case", "steps", "ns/step", "MIPS");
    for (i = 0; i < nc; ++i)
    {
        if (!verify(&cases[i]))
        {
            fails = fails + 1;
            continue;
        }
        steps = run_once(cases[i].prog, cases[i].n, cases[i].mem,
                         cases[i].mem_size, &st);
        measure(&cases[i], steps, &nsps, &mips);
        (void)printf("%-18s %8lu %12.1f %10.1f\n", cases[i].name,
                     (unsigned long)steps, nsps, mips);
        (void)fprintf(csv, "%s,%lu,%.1f,%.1f\n", cases[i].name,
                      (unsigned long)steps, nsps, mips);
    }
    (void)fclose(csv);

    (void)fprintf(stderr, "bench: written bench/bench.csv\n");
    if (fails)
    {
        (void)fprintf(stderr, "bench: %d failures\n", fails);
        return 1;
    }
    (void)printf("bench: ok\n");
    return 0;
}
