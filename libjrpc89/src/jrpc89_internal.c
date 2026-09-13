/* jrpc89_internal.c - shared helpers. */
#include <string.h>

#include <jrpc89.h>

#include "jrpc89_internal.h"

void jrpc89_set_error(j89_arena *a, const char *msg)
{
    size_t cap;
    size_t n;
    cap = J89_ERR_LEN - 1;
    n = strlen(msg);
    if (n > cap)
    {
        n = cap;
    }
    memcpy(a->err, msg, n);
    a->err[n] = '\0';
}

int jrpc89_has_node(j89_len n)
{
    j89_len bad;
    int eq;
    int r;
    bad = J89_BAD;
    eq = (n == bad);
    r = (eq == 0);
    return r;
}
