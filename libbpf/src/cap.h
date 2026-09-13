#ifndef BPF_CAP_H
#define BPF_CAP_H

#include "types.h"
#include "decode.h"

/* Machine-local capability table. Integer guest handles map to a resource
 * type, a rights bitmask, and a host object. Handles are allocated
 * monotonically and never reused during the table's lifetime; exhaustion
 * returns BPF_EFULL instead of wrapping. Revoking a handle invalidates it.
 * Destroying the table invalidates every remaining entry. */

typedef bpf_u32 bpf_handle;

typedef struct bpf_caps bpf_caps;

/* Create a table with `max` distinct handle slots (>= 1 for any allocation). */
bpf_err bpf_caps_create(bpf_u32 max, bpf_caps **out);

void bpf_caps_destroy(bpf_caps *caps);

/* Allocate a fresh handle for (type, rights, host). Returns the handle in
 * *out. Handles are strictly increasing and never reused. */
bpf_err bpf_caps_alloc(bpf_caps *caps, bpf_u32 type, bpf_u32 rights, void *host,
                       bpf_handle *out);

/* Resolve a live handle to its type, rights, and host object. Any output
 * pointer may be null to skip that field. */
bpf_err bpf_caps_lookup(bpf_caps *caps, bpf_handle h, bpf_u32 *type,
                        bpf_u32 *rights, void **host);

/* Invalidate a live handle. */
bpf_err bpf_caps_revoke(bpf_caps *caps, bpf_handle h);

/* Number of currently live handles. */
bpf_u32 bpf_caps_live(const bpf_caps *caps);

#endif
