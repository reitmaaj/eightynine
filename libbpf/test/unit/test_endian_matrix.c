#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte cls;
    bpf_byte source;
    int width;
    bpf_u64 in;
    bpf_u64 out;
} end_case;

static void run_case(const end_case *c)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode = (bpf_byte)((BPF_ALU_END << 4) |
                           (((unsigned)c->source & 1u) << 3) | c->cls);
    in.class = c->cls;
    in.code = BPF_ALU_END;
    in.source = (bpf_byte)((unsigned)c->source & 1u);
    in.mode = 0;
    in.size = 0;
    in.src_reg = 0;
    in.dst_reg = 1;
    in.offset = 0;
    in.imm = c->width;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = c->in;
    e = bpf_alu(&r, &in);
    count = count + 1;
    if (e != BPF_OK || r.r[1] != c->out)
    {
        (void)fprintf(stderr,
                      "FAIL: cls=%u src=%u w=%d in=%lx got=%lx want=%lx"
                      "\n",
                      c->cls, c->source, c->width, c->in, r.r[1], c->out);
        fails = fails + 1;
    }
}

#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define LE BPF_SRC_K
#define BE BPF_SRC_X

static const end_case CASES[] = {
    /* ALU64: unconditional byte swap */
    {A64, LE, 16, 0x1122334455667788UL, 0x0000000000008877UL},
    {A64, LE, 32, 0x1122334455667788UL, 0x0000000088776655UL},
    {A64, LE, 64, 0x1122334455667788UL, 0x8877665544332211UL},
    {A64, LE, 16, 0x0000000000001234UL, 0x0000000000003412UL},
    {A64, LE, 16, 0x00000000000000FFUL, 0x000000000000FF00UL},
    {A64, LE, 32, 0x0000000012345678UL, 0x0000000078563412UL},
    {A64, LE, 64, 0x0001020304050607UL, 0x0706050403020100UL},
    {A64, LE, 64, 0x0000000000000000UL, 0x0000000000000000UL},
    {A64, LE, 64, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL},
    /* ALU BE: byte swap */
    {A32, BE, 16, 0x0000000000001234UL, 0x0000000000003412UL},
    {A32, BE, 32, 0x0000000012345678UL, 0x0000000078563412UL},
    {A32, BE, 64, 0x1122334455667788UL, 0x8877665544332211UL},
    {A32, BE, 32, 0x0000000000000000UL, 0x0000000000000000UL},
    {A32, BE, 32, 0x00000000FFFFFFFFUL, 0x00000000FFFFFFFFUL},
    /* ALU LE: identity */
    {A32, LE, 16, 0x0000000000001234UL, 0x0000000000001234UL},
    {A32, LE, 32, 0x0000000012345678UL, 0x0000000012345678UL},
    {A32, LE, 64, 0x1122334455667788UL, 0x1122334455667788UL},
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
        (void)fprintf(stderr, "test_endian_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_endian_matrix (%d cases)\n", count);
    return 0;
}
