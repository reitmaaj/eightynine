#ifndef BENCH_PROGS_H
#define BENCH_PROGS_H

#include "decode.h"

/* A benchmark program: a builder that emits a decoded instruction vector
 * into `out` (capacity `cap`) and returns the number of instructions written.
 * `mem_size` is the host memory region size the machine needs (0 = none);
 * `n` is filled with the instruction count after building. */
typedef struct bench_prog
{
    const char *name;
    int (*build)(bpf_insn *out, bpf_u32 cap);
    bpf_u32 n;
    bpf_u64 mem_size;
} bench_prog;

int bld_memcpy(bpf_insn *out, bpf_u32 cap);
int bld_arith(bpf_insn *out, bpf_u32 cap);
int bld_jump(bpf_insn *out, bpf_u32 cap);
int bld_mixed(bpf_insn *out, bpf_u32 cap);

extern const bench_prog bench_programs[];

#endif
