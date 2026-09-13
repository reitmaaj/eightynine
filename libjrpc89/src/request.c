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

static void jrpc89_set_member_id(j89_arena *a, j89_len obj, j89_len slot,
                                 const jrpc89_id *id)
{
    j89_len idnode;
    idnode = jrpc89_id_node(a, id);
    j89_object_set(a, obj, slot, "id", 2, idnode);
}

j89_len jrpc89_request_new(j89_arena *a, const char *method, j89_len params,
                           const jrpc89_id *id)
{
    j89_len obj;
    j89_len mlen;
    j89_len mnode;
    j89_len ver;
    j89_len params_slot;
    j89_len id_slot;
    j89_len count;
    int empty;
    int okobj;
    int have_params;
    int have_id;
    empty = jrpc89_method_empty(method);
    if (empty)
    {
        jrpc89_set_error(a, "jrpc89: empty method");
        return J89_BAD;
    }
    count = jrpc89_member_count(params, id);
    obj = j89_object_new(a, count);
    okobj = jrpc89_has_node(obj);
    if (!okobj)
    {
        return J89_BAD;
    }
    ver = j89_string_new(a, "2.0", 3);
    j89_object_set(a, obj, 0, "jsonrpc", 7, ver);
    mlen = strlen(method);
    mnode = j89_string_new(a, method, mlen);
    j89_object_set(a, obj, 1, "method", 6, mnode);
    have_params = jrpc89_has_node(params);
    have_id = jrpc89_id_present(id);
    params_slot = 2;
    if (have_params)
    {
        j89_object_set(a, obj, params_slot, "params", 6, params);
    }
    id_slot = (j89_len)(2 + have_params);
    if (have_id)
    {
        jrpc89_set_member_id(a, obj, id_slot, id);
    }
    return obj;
}
