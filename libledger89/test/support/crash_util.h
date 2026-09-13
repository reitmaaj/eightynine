#ifndef CRASH_UTIL_H
#define CRASH_UTIL_H

/* crash_util.h - shared helpers for the model-filesystem crash matrices. */

#include "model_fs.h"

/* Open a model ledger with the given rotation record target. */
int cu_open(mfs *fs, led89_io *io, ledger89 **l, unsigned long records);

/* Append the canonical pattern record at index. */
int cu_append(ledger89 *l, ledger89_index index);

/* Append the canonical pattern for 1..count. */
int cu_fill(ledger89 *l, ledger89_index count);

/* Append the canonical pattern for first..last. */
int cu_fill_range(ledger89 *l, ledger89_index first, ledger89_index last);

/* Verify that every index in [first,last] reads back as the pattern. */
int cu_check_range(ledger89 *l, ledger89_index first, ledger89_index last);

#endif /* CRASH_UTIL_H */
