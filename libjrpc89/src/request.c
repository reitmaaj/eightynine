/* request.c - build JSON-RPC 2.0 request and notification objects. */
#include <string.h>

#include <jrpc89.h>

#include "id.h"
#include "jrpc89_internal.h"

static int jrpc89_method_empty(const char *method)
{
    if (method == NULL)
    {
        return 1;
    }
    if (method[0] == '\0')
    {
        return 1;
    }
    return 0;
}

static j89_len jrpc89_member_count(j89_len params, const jrpc89_id *id)
{
    j89_len count;
    int have_params;
    int have_id;
    have_params = jrpc89_has_node(params);
    have_id = jrpc89_id_present(id);
    count = (j89_len)(have_params + have_id + 2);
    return count;
}

/* True when a libj89 builder operation has failed the arena. */
static int jrpc89_builder_failed(j89_arena *a)
{
    int failed;
    failed = j89_failed(a);
    return failed;
}

/* Set the jsonrpc and method members. Returns 0 on success, -1 when a
 * libj89 builder operation failed (the arena is then marked failed). */
static int jrpc89_set_common(j89_arena *a, j89_len obj, const char *method)
{
    j89_len ver;
    j89_len mnode;
    j89_len mlen;
    int failed;
    ver = j89_string_new(a, "2.0", 3);
    j89_object_set(a, obj, 0, "jsonrpc", 7, ver);
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        return -1;
    }
    mlen = strlen(method);
    mnode = j89_string_new(a, method, mlen);
    j89_object_set(a, obj, 1, "method", 6, mnode);
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        return -1;
    }
    return 0;
}

/* Set the optional params member. Returns 0 on success, -1 on builder
 * failure. */
static int jrpc89_set_params(j89_arena *a, j89_len obj, j89_len params,
                             int have_params)
{
    int failed;
    if (!have_params)
    {
        return 0;
    }
    j89_object_set(a, obj, 2, "params", 6, params);
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        return -1;
    }
    return 0;
}

/* Set the optional id member. Returns 0 on success, -1 on builder
 * failure. */
static int jrpc89_set_id(j89_arena *a, j89_len obj, j89_len slot,
                         const jrpc89_id *id, int have_id)
{
    j89_len idnode;
    int failed;
    if (!have_id)
    {
        return 0;
    }
    idnode = jrpc89_id_node(a, id);
    j89_object_set(a, obj, slot, "id", 2, idnode);
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        return -1;
    }
    return 0;
}

j89_len jrpc89_request_new(j89_arena *a, const char *method, j89_len params,
                           const jrpc89_id *id)
{
    j89_len obj;
    j89_len count;
    int have_params;
    int have_id;
    int empty;
    int failed;
    int okobj;
    int st;
    if (a == NULL)
    {
        return J89_BAD;
    }
    empty = jrpc89_method_empty(method);
    if (empty)
    {
        jrpc89_set_error(a, "jrpc89: empty method");
        return J89_BAD;
    }
    failed = jrpc89_builder_failed(a);
    if (failed)
    {
        return J89_BAD;
    }
    have_params = jrpc89_has_node(params);
    have_id = jrpc89_id_present(id);
    count = jrpc89_member_count(params, id);
    obj = j89_object_new(a, count);
    okobj = jrpc89_has_node(obj);
    if (!okobj)
    {
        return J89_BAD;
    }
    st = jrpc89_set_common(a, obj, method);
    if (st != 0)
    {
        return J89_BAD;
    }
    st = jrpc89_set_params(a, obj, params, have_params);
    if (st != 0)
    {
        return J89_BAD;
    }
    st = jrpc89_set_id(a, obj, (j89_len)(2 + have_params), id, have_id);
    if (st != 0)
    {
        return J89_BAD;
    }
    return obj;
}
