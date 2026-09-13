#ifndef EINTR_SHIM_H
#define EINTR_SHIM_H

/* eintr_shim.h - linker-wrapped syscall EINTR injection. */

enum
{
    EINTR_PREAD = 0,
    EINTR_PWRITE,
    EINTR_FSYNC,
    EINTR_FDATASYNC,
    EINTR_FTRUNCATE,
    EINTR_RENAME,
    EINTR_UNLINK,
    EINTR_COUNT
};

/* Return EINTR from the (skip+1)-th call of op; later calls delegate. */
void eintr_arm(int op, int skip);
int eintr_fired(void);

#endif /* EINTR_SHIM_H */
