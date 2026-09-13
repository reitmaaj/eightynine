#ifndef REAL_IO_H
#define REAL_IO_H

/* real_io.h - POSIX I/O vtable wrapper that terminates the process at an
 * armed call boundary. Used by the real-filesystem process-kill suite. */

#include "model_fs.h"

typedef struct real_io
{
    led89_io api;
    int crash_op;
    int crash_skip;
    int crash_armed;
} real_io;

void real_io_init(real_io *r);

/* Terminate the process (exit 99) at the (skip+1)-th call of op. */
void real_io_arm(real_io *r, int op, int skip);

const led89_io *real_io_api(real_io *r);

#endif /* REAL_IO_H */
