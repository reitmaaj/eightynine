/* tmpdir.c - fresh temporary directory under build/. */

#include <stdlib.h>
#include <string.h>

#include "tmpdir.h"

int tmpdir_create(char *out, size_t cap)
{
    const char *base;
    size_t n;

    base = "build/ledger89-XXXXXX";
    n = strlen(base);
    if (n + 1u > cap)
    {
        return -1;
    }
    memcpy(out, base, n + 1u);
    if (mkdtemp(out) == NULL)
    {
        return -1;
    }
    return 0;
}
