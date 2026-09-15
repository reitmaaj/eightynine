/* request.c - build and decode JSON-RPC 2.0 request objects. */
#include <jrpc89.h>

#include "id.h"
#include "jrpc89_internal.h"

/* True when the method argument is unusable: NULL with a nonzero length. */
static int jrpc89_method_bad(const char *method, j89_len method_len)
{
    if (method == NULL)
    {
        if (method_len != 0)
        {
            return 1;
        }
    }
    return 0;
}

/* True when params is present but is not an array or an object. JSON-RPC
 * params, when present, must be a structured value. */
static int jrpc89_params_bad(j89_arena *a, j89_len params)
{
    j89_kind k;
    int have;
    int bad;
    have = jrpc89_has_node(params);
    if (!have)
    {
        return 0;
    }
    k = j89_kind_of(a, params);
    bad = 1;
    if (k == J89_ARRAY)
    {
        bad = 0;
    }
    if (k == J89_OBJECT)
    {
        bad = 0;
    }
    return bad;
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

/* Set the jsonrpc and method members. Returns JRPC89_OK on success, or the
 * mapped builder failure status. */
static jrpc89_status jrpc89_set_common(j89_arena *a, j89_len obj,
                                       const char *method, j89_len method_len)
{
    j89_len mnode;
    jrpc89_status st;
    st = jrpc89_set_version(a, obj, 0);
    if (st != JRPC89_OK)
    {
        return st;
    }
    mnode = j89_string_new(a, method, method_len);
    j89_object_set(a, obj, 1, "method", 6, mnode);
    st = jrpc89_builder_check(a);
    return st;
}

/* Set the optional params member. Returns JRPC89_OK on success. */
static jrpc89_status jrpc89_set_params(j89_arena *a, j89_len obj,
                                       j89_len params, int have_params)
{
    jrpc89_status st;
    if (!have_params)
    {
        return JRPC89_OK;
    }
    j89_object_set(a, obj, 2, "params", 6, params);
    st = jrpc89_builder_check(a);
    return st;
}

/* Set the optional id member. Returns JRPC89_OK on success. */
static jrpc89_status jrpc89_set_id(j89_arena *a, j89_len obj, j89_len slot,
                                   const jrpc89_id *id, int have_id)
{
    jrpc89_status st;
    if (!have_id)
    {
        return JRPC89_OK;
    }
    st = jrpc89_set_id_member(a, obj, slot, id);
    return st;
}

jrpc89_status jrpc89_request_new(j89_arena *a, const char *method,
                                 j89_len method_len, j89_len params,
                                 const jrpc89_id *id, j89_len *out)
{
    j89_len obj;
    j89_len count;
    jrpc89_status st;
    int have_params;
    int have_id;
    int bad;
    int valid;
    int dirty;
    int okobj;
    if (a == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (out == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (id == NULL)
    {
        return JRPC89_EINVAL;
    }
    bad = jrpc89_method_bad(method, method_len);
    if (bad)
    {
        return JRPC89_EINVAL;
    }
    valid = jrpc89_id_valid(id);
    if (!valid)
    {
        return JRPC89_EINVAL;
    }
    dirty = jrpc89_arena_dirty(a);
    if (dirty)
    {
        return JRPC89_EINVAL;
    }
    bad = jrpc89_params_bad(a, params);
    if (bad)
    {
        return JRPC89_EINVAL;
    }
    if (method == NULL)
    {
        method = "";
    }
    have_params = jrpc89_has_node(params);
    have_id = jrpc89_id_present(id);
    count = jrpc89_member_count(params, id);
    obj = j89_object_new(a, count);
    okobj = jrpc89_has_node(obj);
    if (!okobj)
    {
        st = jrpc89_status_from_arena(a);
        return st;
    }
    st = jrpc89_set_common(a, obj, method, method_len);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_set_params(a, obj, params, have_params);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_set_id(a, obj, (j89_len)(2 + have_params), id, have_id);
    if (st != JRPC89_OK)
    {
        return st;
    }
    *out = obj;
    return JRPC89_OK;
}

/* ------------------------------------------------------------------ */
/* request decoding                                                    */
/* ------------------------------------------------------------------ */

static void jrpc89_request_zero(jrpc89_request *out)
{
    out->method = NULL;
    out->method_len = 0;
    out->params = J89_BAD;
    out->id.kind = JRPC89_ID_NONE;
    out->id.num = 0;
    out->id.str = NULL;
    out->id.len = 0;
}

/* Decode the required method member: it must be a string. */
static jrpc89_status jrpc89_decode_method(j89_arena *a, j89_len node,
                                          jrpc89_request *out)
{
    j89_len method;
    j89_kind k;
    int has;
    method = j89_object_find(a, node, "method");
    has = jrpc89_has_node(method);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    k = j89_kind_of(a, method);
    if (k != J89_STRING)
    {
        return JRPC89_EPROTO;
    }
    out->method = j89_string_value(a, method);
    out->method_len = j89_string_length(a, method);
    return JRPC89_OK;
}

/* Decode the optional params member: absent, an array, or an object. */
static jrpc89_status jrpc89_decode_params(j89_arena *a, j89_len node,
                                          jrpc89_request *out)
{
    j89_len params;
    int bad;
    int has;
    params = j89_object_find(a, node, "params");
    has = jrpc89_has_node(params);
    if (!has)
    {
        return JRPC89_OK;
    }
    bad = jrpc89_params_bad(a, params);
    if (bad)
    {
        return JRPC89_EPROTO;
    }
    out->params = params;
    return JRPC89_OK;
}

/* Decode the optional id member: absent, integer, string, or null. */
static jrpc89_status jrpc89_decode_optional_id(j89_arena *a, j89_len node,
                                               jrpc89_id *out)
{
    j89_len id;
    jrpc89_status st;
    int has;
    id = j89_object_find(a, node, "id");
    has = jrpc89_has_node(id);
    if (!has)
    {
        return JRPC89_OK;
    }
    st = jrpc89_id_from_node(a, id, out);
    return st;
}

jrpc89_status jrpc89_request_decode(j89_arena *a, j89_len node,
                                    jrpc89_request *out)
{
    jrpc89_request tmp;
    jrpc89_status st;
    int dirty;
    if (a == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (out == NULL)
    {
        return JRPC89_EINVAL;
    }
    if (node == J89_BAD)
    {
        return JRPC89_EPROTO;
    }
    dirty = jrpc89_arena_dirty(a);
    if (dirty)
    {
        return JRPC89_EINVAL;
    }
    st = jrpc89_check_head(a, node);
    if (st != JRPC89_OK)
    {
        return st;
    }
    jrpc89_request_zero(&tmp);
    st = jrpc89_decode_method(a, node, &tmp);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_decode_params(a, node, &tmp);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_decode_optional_id(a, node, &tmp.id);
    if (st != JRPC89_OK)
    {
        return st;
    }
    *out = tmp;
    return JRPC89_OK;
}
