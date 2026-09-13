#include <stdio.h>

#include "decode.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte op;
    bpf_byte regs;
    int off;
    unsigned imm;
    bpf_byte cls;
    bpf_byte code;
    bpf_byte source;
    bpf_byte mode;
    bpf_byte size;
} d_case;

static void put16(bpf_byte *p, int v)
{
    p[0] = (bpf_byte)(v & 0xFF);
    p[1] = (bpf_byte)((v >> 8) & 0xFF);
}

static void put32(bpf_byte *p, unsigned v)
{
    p[0] = (bpf_byte)(v & 0xFFu);
    p[1] = (bpf_byte)((v >> 8) & 0xFFu);
    p[2] = (bpf_byte)((v >> 16) & 0xFFu);
    p[3] = (bpf_byte)((v >> 24) & 0xFFu);
}

static void run_case(const d_case *c)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;

    buf[0] = c->op;
    buf[1] = c->regs;
    put16(buf + 2, c->off);
    put32(buf + 4, c->imm);
    e = bpf_decode(buf, 8, ins, 1, &n);
    count = count + 1;
    if (e != BPF_OK || n != 1)
    {
        (void)fprintf(stderr, "FAIL: op=%02x decode\n", c->op);
        fails = fails + 1;
        return;
    }
    if (ins[0].class != c->cls)
    {
        (void)fprintf(stderr, "FAIL: op=%02x class=%u want=%u\n", c->op,
                      ins[0].class, c->cls);
        fails = fails + 1;
    }
    if (c->mode == 0xFF)
    {
        if (ins[0].code != c->code || ins[0].source != c->source)
        {
            (void)fprintf(stderr, "FAIL: op=%02x code/src\n", c->op);
            fails = fails + 1;
        }
    }
    else
    {
        if (ins[0].mode != c->mode || ins[0].size != c->size)
        {
            (void)fprintf(stderr, "FAIL: op=%02x mode/size\n", c->op);
            fails = fails + 1;
        }
    }
    if (ins[0].src_reg != (bpf_byte)(c->regs >> 4) ||
        ins[0].dst_reg != (bpf_byte)(c->regs & 0x0Fu))
    {
        (void)fprintf(stderr, "FAIL: op=%02x regs\n", c->op);
        fails = fails + 1;
    }
    if (ins[0].offset != c->off)
    {
        (void)fprintf(stderr, "FAIL: op=%02x offset=%d want=%d\n", c->op,
                      ins[0].offset, c->off);
        fails = fails + 1;
    }
}

#define ALU BPF_CLS_ALU
#define ALU64 BPF_CLS_ALU64
#define JMP BPF_CLS_JMP
#define JMP32 BPF_CLS_JMP32
#define LD BPF_CLS_LD
#define LDX BPF_CLS_LDX
#define ST BPF_CLS_ST
#define STX BPF_CLS_STX
#define ALU_ BPF_MODE_ABS

static const d_case CASES[] = {
    /* ALU class: code=op>>4, source=(op>>3)&1 */
    {0x04, 0x10, 0, 0, ALU, 0x0, 0, 0xFF, 0},
    {0x0f, 0x10, 0, 0, ALU64, 0x0, 0x1, 0xFF, 0},
    {0xb7, 0x01, -1, 0xFFFFFFFFu, ALU64, 0xb, 0, 0xFF, 0},
    {0xbf, 0x12, 0, 0, ALU64, 0xb, 0x1, 0xFF, 0},
    {0xd4, 0, 0, 0, ALU, 0xd, 0, 0xFF, 0},
    /* JMP / JMP32 */
    {0x05, 0, 2, 0, JMP, 0x0, 0, 0xFF, 0},
    {0x15, 0, 0, 0, JMP, 0x1, 0, 0xFF, 0},
    {0xce, 0, 0, 0, JMP32, 0xc, 0x1, 0xFF, 0},
    {0x95, 0, 0, 0, JMP, 0x9, 0, 0xFF, 0},
    /* LD/ST: mode=op>>5, size=(op>>3)&3 */
    {0x71, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEM, BPF_SIZE_B},
    {0x69, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEM, BPF_SIZE_H},
    {0x61, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEM, BPF_SIZE_W},
    {0x79, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEM, BPF_SIZE_DW},
    {0x91, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEMSX, BPF_SIZE_B},
    {0x81, 0, 0, 0, LDX, 0, 0, BPF_MODE_MEMSX, BPF_SIZE_W},
    {0x73, 0, 0, 0, STX, 0, 0, BPF_MODE_MEM, BPF_SIZE_B},
    {0x63, 0, 0, 0, STX, 0, 0, BPF_MODE_MEM, BPF_SIZE_W},
    {0x7b, 0, 0, 0, STX, 0, 0, BPF_MODE_MEM, BPF_SIZE_DW},
    {0x72, 0, 0, 0, ST, 0, 0, BPF_MODE_MEM, BPF_SIZE_B},
    {0x7a, 0, 0, 0, ST, 0, 0, BPF_MODE_MEM, BPF_SIZE_DW},
    {0xc3, 0, 0, 0, STX, 0, 0, BPF_MODE_ATOMIC, BPF_SIZE_W},
    {0xdb, 0, 0, 0, STX, 0, 0, BPF_MODE_ATOMIC, BPF_SIZE_DW},
    {0x20, 0, 0, 0, LD, 0, 0, BPF_MODE_ABS, BPF_SIZE_W},
    {0x28, 0, 0, 0, LD, 0, 0, BPF_MODE_ABS, BPF_SIZE_H},
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
        (void)fprintf(stderr, "test_decode_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_decode_matrix (%d cases)\n", count);
    return 0;
}
