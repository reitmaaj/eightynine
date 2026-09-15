/* jrpc89_internal.c - shared helpers. */
#include <jrpc89.h>

#include "jrpc89_internal.h"

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

int jrpc89_arena_dirty(j89_arena *a)
{
    const char *e;
    int failed;
    failed = j89_failed(a);
    if (failed)
    {
        return 1;
    }
    e = j89_error(a);
    if (e[0] != '\0')
    {
        return 1;
    }
    return 0;
}

jrpc89_status jrpc89_status_from_arena(j89_arena *a)
{
    const char *e;
    e = j89_error(a);
    if (e[0] != '\0')
    {
        return JRPC89_EINVAL;
    }
    return JRPC89_ENOMEM;
}
