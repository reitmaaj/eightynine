#ifndef CRASH_UTIL_H
#define CRASH_UTIL_H

/* crash_util.h - shared helpers for the model-filesystem crash matrices. */

#include "model_fs.h"

/* Open a fresh model ledger. */
int cu_open(mfs *fs, led89_io *io, ledger89 **l);

/* Reopen after a crash and capture state; returns 0 on failure. */
int cu_reopen(mfs *fs, led89_io *io, ledger89 **l, ledger89_state *st);

/* Lower the automatic rotation target (white-box, tests only). */
void cu_set_target(ledger89 *l, unsigned long bytes);

/* Append the canonical one-byte record for logical index value. */
int cu_append(ledger89 *l, unsigned long value);

/* Append the canonical records for 1..count. */
int cu_fill(ledger89 *l, unsigned long count);

/* Append the canonical records for first..last. */
int cu_fill_range(ledger89 *l, unsigned long first, unsigned long last);

/* Verify every index in [first,last] reads back as the canonical byte. */
int cu_check_range(ledger89 *l, unsigned long first, unsigned long last);

#endif /* CRASH_UTIL_H */
