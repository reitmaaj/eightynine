#ifndef TMPDIR_H
#define TMPDIR_H

/* tmpdir.h - create a fresh temporary directory under build/ for tests. */

#include <stddef.h>

/* Copy a fresh template into out (must hold at least 24 bytes) and create
 * the directory. Returns 0 on success, -1 on failure. */
int tmpdir_create(char *out, size_t cap);

#endif /* TMPDIR_H */
