#include "progs.h"

/* Builder for a block-copy loop: copy a source region onto a destination
 * region inside the host memory area, then EXIT with r0 = 0. Exercises the
 * LDX/STX MEM (double-word) path inside a bounded loop. */
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

#define MEM_BLOCKS 200
#define MEM_DST_BASE 1000UL

int bld_memcpy(bpf_insn *out, bpf_u32 cap)
{
    bpf_u32 p;

    p = 0;
    (void)cap;
    out[p++] = mk(0xb7, 0, 0, 0, 0);           /* r0 = src base 0 */
    out[p++] = mk(0xb7, 0, 1, 1000, 0);        /* r1 = dst base  */
    out[p++] = mk(0xb7, 0, 2, MEM_BLOCKS, 0);  /* r2 = block count */
    /* loop: */
    out[p++] = mk(0x79, 1, 3, 0, 0);           /* r3 = [r0+0] DW */
    out[p++] = mk(0x79, 1, 4, 8, 0);           /* r4 = [r0+8] DW */
    out[p++] = mk(0x7b, 3, 1, 0, 0);           /* [r1+0] = r3 */
    out[p++] = mk(0x7b, 4, 1, 8, 0);           /* [r1+8] = r4 */
    out[p++] = mk(0x07, 0, 0, 16, 0);          /* r0 += 16 */
    out[p++] = mk(0x07, 0, 1, 16, 0);          /* r1 += 16 */
    out[p++] = mk(0x17, 0, 2, 1, 0);           /* r2 -= 1 */
    out[p++] = mk(0x56, 0, 2, 0, -7);          /* if r2 != 0 goto loop */
    out[p++] = mk(0x95, 0, 0, 0, 0);           /* EXIT */
    return (int)p;
}

/* Arithmetic-heavy loop: multiply/add/xor a seed, decrement a counter.
 * Exercises ALU64 K immediates inside a bounded loop. */
#define ARITH_COUNT 15000

int bld_arith(bpf_insn *out, bpf_u32 cap)
{
    bpf_u32 p;

    p = 0;
    (void)cap;
    out[p++] = mk(0xb7, 0, 0, 1, 0);              /* r0 = 1 */
    out[p++] = mk(0xb7, 0, 1, ARITH_COUNT, 0);    /* r1 = count */
    /* loop: */
    out[p++] = mk(0x27, 0, 0, 1103515245, 0);     /* r0 *= 1103515245 */
    out[p++] = mk(0x07, 0, 0, 12345, 0);          /* r0 += 12345 */
    out[p++] = mk(0xa7, 0, 0, 1234567, 0);        /* r0 ^= 1234567 */
    out[p++] = mk(0x17, 0, 1, 1, 0);              /* r1 -= 1 */
    out[p++] = mk(0x56, 0, 1, 0, -5);             /* if r1 != 0 goto loop */
    out[p++] = mk(0x95, 0, 0, 0, 0);              /* EXIT */
    return (int)p;
}

/* Branch-heavy loop: compare a counter with several JMP32 conditions and a
 * JA jump, then decrement. Exercises 32-bit conditional branches. */
#define JUMP_COUNT 15000

int bld_jump(bpf_insn *out, bpf_u32 cap)
{
    bpf_u32 p;

    p = 0;
    (void)cap;
    out[p++] = mk(0xb7, 0, 0, 0, 0);              /* r0 = 0 */
    out[p++] = mk(0xb7, 0, 1, JUMP_COUNT, 0);     /* r1 = count */
    /* loop: */
    out[p++] = mk(0x76, 0, 1, 0, 1);              /* if r1 > 0 goto dec (taken) */
    out[p++] = mk(0x05, 0, 0, 0, 2);              /* JA goto done */
    /* dec: */
    out[p++] = mk(0x17, 0, 1, 1, 0);              /* r1 -= 1 */
    out[p++] = mk(0x05, 0, 0, 0, -4);             /* JA goto loop */
    /* done: */
    out[p++] = mk(0x95, 0, 0, 0, 0);              /* EXIT */
    return (int)p;
}

/* Mixed workload: word load, ALU, store and a loop counter. */
#define MIX_COUNT 8000

int bld_mixed(bpf_insn *out, bpf_u32 cap)
{
    bpf_u32 p;

    p = 0;
    (void)cap;
    out[p++] = mk(0xb7, 0, 0, 0, 0);              /* r0 = addr 0 */
    out[p++] = mk(0xb7, 0, 2, MIX_COUNT, 0);      /* r2 = count */
    /* loop: */
    out[p++] = mk(0x61, 1, 3, 0, 0);              /* r3 = [r0+0] W */
    out[p++] = mk(0x07, 0, 3, 1, 0);              /* r3 += 1 */
    out[p++] = mk(0x63, 1, 3, 0, 0);              /* [r0+0] = r3 */
    out[p++] = mk(0x07, 0, 0, 4, 0);              /* r0 += 4 */
    out[p++] = mk(0x17, 0, 2, 1, 0);              /* r2 -= 1 */
    out[p++] = mk(0x56, 0, 2, 0, -6);             /* if r2 != 0 goto loop */
    out[p++] = mk(0x95, 0, 0, 0, 0);              /* EXIT */
    return (int)p;
}

const bench_prog bench_programs[] = {
    {"memcpy", bld_memcpy, 0, MEM_DST_BASE + MEM_BLOCKS * 16UL},
    {"arith", bld_arith, 0, 0},
    {"jump", bld_jump, 0, 0},
    {"mixed", bld_mixed, 0, 8192UL},
    {0, 0, 0, 0},
};
