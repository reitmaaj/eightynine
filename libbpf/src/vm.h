#ifndef BPF_VM_H
#define BPF_VM_H

#include "mem.h"
#include "program.h"
#include "eval.h"
#include "cap.h"

/* Opaque, owned machine that executes a validated bpf_program over its own
 * guest address space. It steps instruction by instruction, follows the
 * branch/local-call targets resolved at load time, reaches guest memory only
 * through the region resolver, and reuses the tested ALU and condition
 * helpers. Register 10 (the frame pointer) is VM-managed and never written by
 * the guest. */

#define BPF_VM_CALL_DEPTH 64

typedef enum bpf_vm_state
{
    BPF_VM_RUNNING = 0, /* more work; step again */
    BPF_VM_RETURNED,    /* terminal: r0 holds the result */
    BPF_VM_WAITING,     /* helper pending; completion expected */
    BPF_VM_TRAPPED,     /* terminal: fault or denied guest access */
    BPF_VM_EXHAUSTED    /* step budget consumed */
} bpf_vm_state;

typedef struct bpf_vm bpf_vm;

/* Create a machine over an immutable program with a fresh owned guest address
 * space built from `ncfg` region configurations. The input region becomes the
 * entry context (r1 = its guest base, r2 = its length); the stack region base
 * becomes the frame pointer r10. All other registers are zeroed. */
bpf_err bpf_vm_create(const bpf_program *prog, const bpf_region_cfg *cfg,
                      bpf_u32 ncfg, bpf_u64 budget, bpf_vm **out);

void bpf_vm_destroy(bpf_vm *vm);

/* Execute one instruction and advance, or report a terminal state. A machine
 * that has already returned or trapped stays terminal. */
bpf_vm_state bpf_vm_step(bpf_vm *vm);

bpf_vm_state bpf_vm_get_state(const bpf_vm *vm);

/* Read-only snapshot of a register (0..10). */
bpf_u64 bpf_vm_reg(const bpf_vm *vm, bpf_u32 idx);

/* Replace the remaining instruction budget. */
void bpf_vm_set_budget(bpf_vm *vm, bpf_u64 budget);

/* Register a live capability (type, rights, host object) and return its
 * handle, owned by the machine's capability table. */
bpf_err bpf_vm_reg_cap(bpf_vm *vm, bpf_u32 type, bpf_u32 rights, void *host,
                       bpf_handle *out);

/* Invalidate a registered capability handle. */
bpf_err bpf_vm_revoke_cap(bpf_vm *vm, bpf_handle h);

/* Gate helper (import) calls on a live capability in r1 of the given type and
 * with the given permission bits. When set, a helper call traps unless r1
 * holds a live handle granting them. */
bpf_err bpf_vm_set_import_gate(bpf_vm *vm, bpf_u32 type, bpf_u32 rights);

/* When the machine is WAITING on a helper call, the helper (import) id. */
bpf_u32 bpf_vm_pending_import(const bpf_vm *vm);

/* Complete the pending helper request with `status`. Only valid while the
 * machine is WAITING: sets r0 to `status`, clears caller-clobbered r1-r5, and
 * resumes at the instruction after the helper call. Returns BPF_ESTALE and
 * leaves the machine unchanged if no request is pending. */
bpf_err bpf_vm_complete(bpf_vm *vm, bpf_u64 status);

#endif
