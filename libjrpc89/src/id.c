/* id.c - JSON-RPC 2.0 id handling. */
#include <math.h>

#include <jrpc89.h>

#include "id.h"
#include "jrpc89_internal.h"

/* Exact-integer bound shared with libj89: every accepted integer |n| <= 2^53
 * is exactly representable as a double. */
#define JRPC89_INT_EXACT_MAX 9007199254740992.0

static int jrpc89_bytes_equal(const char *x, j89_len xlen, const char *y,
                              j89_len ylen)
{
    j89_len i;
    if (xlen != ylen)
    {
        return 0;
    }
    for (i = 0; i < xlen; i = i + 1)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

/* True when v is an exact integer in libj89's representable domain. */
static int jrpc89_int_exact(j89_int v)
{
    j89_int fl;
    int isint;
    int inrange;
    if (v != v)
    {
        return 0;
    }
    fl = floor(v);
    isint = 0;
    if (v == fl)
    {
        isint = 1;
    }
    inrange = 0;
    if (v >= -JRPC89_INT_EXACT_MAX)
    {
        if (v <= JRPC89_INT_EXACT_MAX)
        {
            inrange = 1;
        }
    }
    if (!isint)
    {
        return 0;
    }
    if (!inrange)
    {
        return 0;
    }
    return 1;
}

int jrpc89_id_present(const jrpc89_id *id)
{
    int present;
    present = 0;
    if (id != NULL)
    {
        if (id->kind != JRPC89_ID_NONE)
        {
            present = 1;
        }
    }
    return present;
}

int jrpc89_id_valid(const jrpc89_id *id)
{
    int exact;
    if (id == NULL)
    {
        return 0;
    }
    if (id->kind == JRPC89_ID_NONE)
    {
        return 1;
    }
    if (id->kind == JRPC89_ID_INT)
    {
        exact = jrpc89_int_exact(id->num);
        return exact;
    }
    if (id->kind == JRPC89_ID_STRING)
    {
        if (id->str == NULL)
        {
            return 0;
        }
        return 1;
    }
    if (id->kind == JRPC89_ID_NULL)
    {
        return 1;
    }
    return 0;
}

int jrpc89_id_matches(const jrpc89_id *a, const jrpc89_id *b)
{
    if (a->kind != b->kind)
    {
        return 0;
    }
    if (a->kind == JRPC89_ID_INT)
    {
        int eq;
        eq = 0;
        if (a->num == b->num)
        {
            eq = 1;
        }
        return eq;
    }
    if (a->kind == JRPC89_ID_STRING)
    {
        int eq;
        eq = jrpc89_bytes_equal(a->str, a->len, b->str, b->len);
        return eq;
    }
    return 1;
}

j89_len jrpc89_id_node(j89_arena *a, const jrpc89_id *id)
{
    j89_len node;
    node = J89_BAD;
    if (id->kind == JRPC89_ID_INT)
    {
        node = j89_integer_new(a, id->num);
    }
    if (id->kind == JRPC89_ID_STRING)
    {
        node = j89_string_new(a, id->str, id->len);
    }
    if (id->kind == JRPC89_ID_NULL)
    {
        node = j89_null_new(a);
    }
    return node;
}
