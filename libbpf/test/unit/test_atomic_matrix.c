#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte size;
    unsigned op;
    int fetch;
    bpf_u64 old;
    bpf_u64 src;
    bpf_u64 r0;
    bpf_u64 new;
    bpf_u64 reg; /* expected src_reg or r0 result */
} at_case;

static void run_case(const at_case *c)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[16];
    int i;
    unsigned opval;
    bpf_u64 want;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    if (c->size == BPF_SIZE_W)
    {
        mem[0] = (bpf_byte)(c->old & 0xFFu);
        mem[1] = (bpf_byte)((c->old >> 8) & 0xFFu);
        mem[2] = (bpf_byte)((c->old >> 16) & 0xFFu);
        mem[3] = (bpf_byte)((c->old >> 24) & 0xFFu);
    }
    else
    {
        int n;

        for (n = 0; n < 8; ++n)
        {
            mem[n] = (bpf_byte)((c->old >> (8 * n)) & 0xFFu);
        }
    }
    in.opcode = (bpf_byte)((BPF_MODE_ATOMIC << 5) | ((unsigned)c->size << 3) |
                           BPF_CLS_STX);
    in.class = BPF_CLS_STX;
    in.mode = BPF_MODE_ATOMIC;
    in.size = c->size;
    in.code = 0;
    in.source = 0;
    in.src_reg = 2;
    in.dst_reg = 1;
    in.offset = 0;
    in.imm = 0;
    opval = c->op;
    if (c->fetch)
    {
        opval = opval | 0x01u;
    }
    in.imm = (int)opval;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_machine_init(&m, &in, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0;
    m.regs.r[2] = c->src;
    m.regs.r[0] = c->r0;
    st = bpf_step(&m);
    count = count + 1;
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: atomic status %d\n", (int)st);
        fails = fails + 1;
        return;
    }
    want = c->new & 0xFFFFFFFFu;
    if (c->size == BPF_SIZE_DW)
    {
        want = c->new;
    }
    if (mem[0] != (bpf_byte)(want & 0xFFu) ||
        mem[1] != (bpf_byte)((want >> 8) & 0xFFu))
    {
        (void)fprintf(stderr, "FAIL: atomic mem[0..1]=%02x%02x want low=%lx\n",
                      mem[1], mem[0], want);
        fails = fails + 1;
    }
    /* CMPXCHG puts result in r0; simple/XCHG with fetch in src reg. */
    if ((c->op & 0xF0u) == 0xF0u)
    {
        if (m.regs.r[0] != c->reg)
        {
            (void)fprintf(stderr, "FAIL: cmpxchg r0=%lx want=%lx\n",
                          m.regs.r[0], c->reg);
            fails = fails + 1;
        }
    }
    else if (c->fetch)
    {
        if (m.regs.r[2] != c->reg)
        {
            (void)fprintf(stderr, "FAIL: fetch src=%lx want=%lx\n", m.regs.r[2],
                          c->reg);
            fails = fails + 1;
        }
    }
}

#define OP_ADD 0x00u
#define OP_OR 0x40u
#define OP_AND 0x50u
#define OP_XOR 0xa0u
#define OP_XCHG 0xe0u
#define OP_CMP 0xf0u

static const at_case CASES[] = {
    /* ADD W */
    {BPF_SIZE_W, OP_ADD, 0, 10, 3, 0, 13, 0},
    {BPF_SIZE_W, OP_ADD, 1, 10, 3, 0, 13, 10},
    {BPF_SIZE_W, OP_ADD, 0, 0xFFFFFFFFUL, 1, 0, 0, 0},
    /* OR W */
    {BPF_SIZE_W, OP_OR, 0, 0x0F, 0xF0, 0, 0xFF, 0},
    {BPF_SIZE_W, OP_OR, 1, 0x0F, 0xF0, 0, 0xFF, 0x0F},
    /* AND W */
    {BPF_SIZE_W, OP_AND, 0, 0x0F, 0x33, 0, 0x03, 0},
    {BPF_SIZE_W, OP_AND, 1, 0x0F, 0x33, 0, 0x03, 0x0F},
    /* XOR W */
    {BPF_SIZE_W, OP_XOR, 0, 0x0F, 0x33, 0, 0x3C, 0},
    {BPF_SIZE_W, OP_XOR, 1, 0x0F, 0x33, 0, 0x3C, 0x0F},
    /* XCHG W */
    {BPF_SIZE_W, OP_XCHG, 1, 7, 9, 0, 9, 7},
    /* CMPXCHG W match */
    {BPF_SIZE_W, OP_CMP, 1, 5, 9, 5, 9, 5},
    /* CMPXCHG W mismatch */
    {BPF_SIZE_W, OP_CMP, 1, 5, 9, 6, 5, 5},
    /* ADD DW */
    {BPF_SIZE_DW, OP_ADD, 0, 0x100000000UL, 0x200000000UL, 0, 0x300000000UL, 0},
    {BPF_SIZE_DW, OP_ADD, 1, 0x100000000UL, 1, 0, 0x100000001UL, 0x100000000UL},
    /* OR DW */
    {BPF_SIZE_DW, OP_OR, 0, 0x0F, 0xF0, 0, 0xFF, 0},
    /* AND DW */
    {BPF_SIZE_DW, OP_AND, 0, 0xFFFFFFFFUL, 0x0F, 0, 0x0F, 0},
    /* XOR DW */
    {BPF_SIZE_DW, OP_XOR, 0, 0x0F, 0x0F, 0, 0, 0},
    /* XCHG DW */
    {BPF_SIZE_DW, OP_XCHG, 1, 0x0000000100000001UL, 0xFFFFFFFFFFFFFFFFUL, 0,
     0xFFFFFFFFFFFFFFFFUL, 0x0000000100000001UL},
    /* CMPXCHG DW match */
    {BPF_SIZE_DW, OP_CMP, 1, 0x123456789ABCDEF0UL, 0xAAAAAAAAAAAAAAAAUL,
     0x123456789ABCDEF0UL, 0xAAAAAAAAAAAAAAAAUL, 0x123456789ABCDEF0UL},
    /* CMPXCHG DW mismatch */
    {BPF_SIZE_DW, OP_CMP, 1, 0x1111111111111111UL, 0x2222222222222222UL,
     0x3333333333333333UL, 0x1111111111111111UL, 0x1111111111111111UL},
};

int main(void)
{
    unsigned i;

    for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i)
    {
        run_case(&CASES[i]);
    }
    if (fails)
    {
        (void)fprintf(stderr, "test_atomic_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_atomic_matrix (%d cases)\n", count);
    return 0;
}
