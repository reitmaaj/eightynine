#ifndef NOMEM_SHIM_H
#define NOMEM_SHIM_H

/* nomem_shim.h - linker-wrapped allocator failure injection. */

/* Fail the (skip+1)-th wrapped allocation; later allocations succeed. */
void nomem_arm(int skip);
void nomem_disarm(void);
int nomem_fired(void);

#endif /* NOMEM_SHIM_H */
