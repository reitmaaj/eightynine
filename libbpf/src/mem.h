#ifndef BPF_MEM_H
#define BPF_MEM_H

#include "types.h"
#include "decode.h"

/* Owned guest address space (sandbox Phase 2). A machine owns a fixed,
 * non-overlapping table of permission-checked regions. Guest address zero is
 * reserved and unmapped. Every guest access is validated through one resolver
 * before any host pointer arithmetic. */

#define BPF_MEM_MAX 4

/* Access bits used by the resolver and stored per region. */
#define BPF_MEM_R 0x01u
#define BPF_MEM_W 0x02u

/* Region kinds; INPUT and CONST are read-only, MEM and STACK read/write. */
#define BPF_RINPUT 0
#define BPF_RCONST 1
#define BPF_RMEM 2
#define BPF_RSTACK 3

/* One region's configuration. `init` (length `init_len`) is copied into the
 * host backing when supplied; otherwise the backing is zeroed. */
typedef struct bpf_region_cfg
{
    bpf_byte kind;
    bpf_off64 guest; /* guest base address; must be non-zero */
    bpf_off64 len;   /* region length in bytes */
    const bpf_byte *init;
    bpf_u32 init_len;
} bpf_region_cfg;

typedef struct bpf_memory bpf_memory;

/* Construct an owned region table from `n` configurations (at most
 * BPF_MEM_MAX). Rejects overlapping or overflowing mappings and any mapping
 * that covers guest address zero. On success *out owns the memory; the caller
 * must destroy it. */
bpf_err bpf_memory_create(const bpf_region_cfg *cfg, bpf_u32 n,
                          bpf_memory **out);

void bpf_memory_destroy(bpf_memory *m);

bpf_u32 bpf_memory_nregions(const bpf_memory *m);

/* Resolve the whole interval [addr, addr + bytes) for the required access
 * bits `acc`. Returns BPF_OK and sets *host to the region-relative host
 * backing pointer only when the interval fits one region, that region permits
 * `acc`, and no host arithmetic overflow is possible. Any other result leaves
 * *host untouched and reports BPF_EADDR or BPF_EPERM. */
bpf_err bpf_mem_resolve(bpf_memory *m, bpf_off64 addr, bpf_off64 bytes,
                        bpf_byte acc, bpf_byte **host);

/* Bytewise little-endian access over the resolver. `width` is 1, 2, 4, or 8.
 * The exact byte interval is resolved (read for loads, write for stores)
 * before any host byte is touched, so a rejected access changes nothing. */
bpf_err bpf_mem_load(bpf_memory *m, bpf_off64 addr, bpf_byte width,
                     bpf_u64 *value);
bpf_err bpf_mem_store(bpf_memory *m, bpf_off64 addr, bpf_byte width,
                      bpf_u64 value);

#endif
