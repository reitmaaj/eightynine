#ifndef BPF_EVAL_H
#define BPF_EVAL_H

#include "decode.h"

/* Register file: r0..r10. r10 is the read-only frame pointer, r0 the return
 * value, r1..r5 the standard argument registers. */
typedef struct bpf_regs
{
    bpf_u64 r[BPF_NREG];
} bpf_regs;

/* Execute one ALU/ALU64 arithmetic or byte-swap instruction (RFC 9669
 * sections 4.1, 4.2) over the register file. */
bpf_err bpf_alu(bpf_regs *regs, const bpf_insn *in);

/* 1 if a JMP/JMP32 condition code `code` is taken for operands d (register)
 * and s (source). `is32` selects 32-bit (low-word) semantics. */
bpf_byte bpf_cond_taken(bpf_byte code, bpf_byte is32, bpf_u64 d, bpf_u64 s);

/* Fixed-size call stack for program-local functions (RFC 9669 section
 * 4.3.2). A deeper call chain is reported as an error. */
#define BPF_STACK_N 64

typedef enum bpf_status
{
    BPF_STAT_RUNNING = 0, /* more work; step again */
    BPF_STAT_RETURNED,    /* EXIT at top level; r0 holds the result */
    BPF_STAT_HOSTCALL,    /* helper call; `helper` holds the id */
    BPF_STAT_EXHAUSTED,   /* step budget exhausted */
    BPF_STAT_TRAP,        /* out-of-bounds memory access */
    BPF_STAT_ERR          /* fell off program / call stack overflow */
} bpf_status;

typedef struct bpf_machine
{
    bpf_regs regs;
    const bpf_insn *prog;
    bpf_u32 n;
    bpf_u32 pc;
    bpf_u32 stack[BPF_STACK_N];
    bpf_u32 stack_n;
    bpf_i64 budget;
    bpf_u32 helper; /* pending helper id for BPF_STAT_HOSTCALL */
    bpf_byte halted;
    bpf_byte *mem;    /* host-provided memory region for load/store */
    bpf_u64 mem_size; /* size of the memory region in bytes */
} bpf_machine;

/* Initialize a machine over a decoded program with an empty call stack. */
void bpf_machine_init(bpf_machine *m, const bpf_insn *prog, bpf_u32 n);

/* Execute one instruction at m->pc and advance the machine. Returns
 * RUNNING for more work; a terminal status otherwise. */
bpf_status bpf_step(bpf_machine *m);

#endif
