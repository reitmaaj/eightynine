/* response.c - decode and build JSON-RPC 2.0 response objects. */
#include <jrpc89.h>

#include "id.h"
#include "jrpc89_internal.h"

static void jrpc89_error_zero(jrpc89_error *out)
{
    out->code = 0;
    out->message = "";
    out->message_len = 0;
    out->data = J89_BAD;
}

/* Initialize every field of the decode result before any optional part is
 * filled in, so a partially built view never escapes. */
static void jrpc89_response_zero(jrpc89_response *out)
{
    out->kind = JRPC89_RESPONSE_RESULT;
    out->id.kind = JRPC89_ID_NONE;
    out->id.num = 0;
    out->id.str = NULL;
    out->id.len = 0;
    out->result = J89_BAD;
    jrpc89_error_zero(&out->error);
}

/* Decode the error member: an object with an integer code and a string
 * message; data is optional. Returns JRPC89_OK or JRPC89_EPROTO. */
static jrpc89_status jrpc89_decode_error(j89_arena *a, j89_len resp,
                                         jrpc89_error *out)
{
    j89_len err;
    j89_len code;
    j89_len msg;
    j89_kind k;
    int has;
    err = j89_object_find(a, resp, "error");
    k = j89_kind_of(a, err);
    if (k != J89_OBJECT)
    {
        return JRPC89_EPROTO;
    }
    code = j89_object_find(a, err, "code");
    has = jrpc89_has_node(code);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    k = j89_kind_of(a, code);
    if (k != J89_INTEGER)
    {
        return JRPC89_EPROTO;
    }
    msg = j89_object_find(a, err, "message");
    has = jrpc89_has_node(msg);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    k = j89_kind_of(a, msg);
    if (k != J89_STRING)
    {
        return JRPC89_EPROTO;
    }
    out->code = j89_int_value(a, code);
    out->message = j89_string_value(a, msg);
    out->message_len = j89_string_length(a, msg);
    out->data = j89_object_find(a, err, "data");
    return JRPC89_OK;
}

/* Exactly one of result or error must be present. */
static jrpc89_status jrpc89_check_xor(int have_result, int have_error)
{
    if (have_result)
    {
        if (have_error)
        {
            return JRPC89_EPROTO;
        }
        return JRPC89_OK;
    }
    if (have_error)
    {
        return JRPC89_OK;
    }
    return JRPC89_EPROTO;
}

static jrpc89_status jrpc89_decode_error_body(j89_arena *a, j89_len node,
                                              jrpc89_response *out)
{
    jrpc89_status st;
    st = jrpc89_decode_error(a, node, &out->error);
    if (st != JRPC89_OK)
    {
        return st;
    }
    out->kind = JRPC89_RESPONSE_ERROR;
    return JRPC89_OK;
}

static jrpc89_status jrpc89_decode_result_body(j89_arena *a, j89_len node,
                                               jrpc89_response *out)
{
    j89_len result;
    result = j89_object_find(a, node, "result");
    out->result = result;
    out->kind = JRPC89_RESPONSE_RESULT;
    return JRPC89_OK;
}

/* Fill in the result-or-error part of the view. */
static jrpc89_status jrpc89_decode_body(j89_arena *a, j89_len node,
                                        jrpc89_response *out)
{
    jrpc89_status st;
    int have_result;
    int have_error;
    have_result = jrpc89_member_has(a, node, "result");
    have_error = jrpc89_member_has(a, node, "error");
    st = jrpc89_check_xor(have_result, have_error);
    if (st != JRPC89_OK)
    {
        return st;
    }
    if (have_error)
    {
        st = jrpc89_decode_error_body(a, node, out);
        return st;
    }
    st = jrpc89_decode_result_body(a, node, out);
    return st;
}

/* Decode the id member: it must be present and be an integer, string, or
 * null. Returns JRPC89_OK or JRPC89_EPROTO. */
static jrpc89_status jrpc89_decode_id(j89_arena *a, j89_len resp,
                                      jrpc89_id *out)
{
    j89_len id;
    jrpc89_status st;
    int has;
    id = j89_object_find(a, resp, "id");
    has = jrpc89_has_node(id);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    st = jrpc89_id_from_node(a, id, out);
    return st;
}

jrpc89_status jrpc89_response_decode(j89_arena *a, j89_len node,
                                     jrpc89_response *out)
{
    jrpc89_response tmp;
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
    jrpc89_response_zero(&tmp);
    st = jrpc89_decode_body(a, node, &tmp);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_decode_id(a, node, &tmp.id);
    if (st != JRPC89_OK)
    {
        return st;
    }
    *out = tmp;
    return JRPC89_OK;
}

/* ------------------------------------------------------------------ */
/* response construction                                               */
/* ------------------------------------------------------------------ */

/* Shared argument checks for both response builders. */
static jrpc89_status jrpc89_response_args_check(j89_arena *a,
                                                const jrpc89_id *id,
                                                const j89_len *out)
{
    int valid;
    int dirty;
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
    if (id->kind == JRPC89_ID_NONE)
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
    return JRPC89_OK;
}

static jrpc89_status jrpc89_error_message_check(const char *message,
                                                j89_len message_len)
{
    if (message == NULL)
    {
        if (message_len != 0)
        {
            return JRPC89_EINVAL;
        }
    }
    return JRPC89_OK;
}

static j89_len jrpc89_error_member_count(j89_len data)
{
    j89_len count;
    int have_data;
    have_data = jrpc89_has_node(data);
    count = (j89_len)(have_data + 2);
    return count;
}

static jrpc89_status jrpc89_set_data(j89_arena *a, j89_len err, j89_len data)
{
    jrpc89_status st;
    j89_object_set(a, err, 2, "data", 4, data);
    st = jrpc89_builder_check(a);
    return st;
}

/* Fill the error object: code, message, and optional data. */
static jrpc89_status jrpc89_fill_error(j89_arena *a, j89_len err, j89_int code,
                                       const char *message, j89_len message_len,
                                       j89_len data)
{
    j89_len cnode;
    j89_len mnode;
    jrpc89_status st;
    int have_data;
    cnode = j89_integer_new(a, code);
    j89_object_set(a, err, 0, "code", 4, cnode);
    st = jrpc89_builder_check(a);
    if (st != JRPC89_OK)
    {
        return st;
    }
    mnode = j89_string_new(a, message, message_len);
    j89_object_set(a, err, 1, "message", 7, mnode);
    st = jrpc89_builder_check(a);
    if (st != JRPC89_OK)
    {
        return st;
    }
    have_data = jrpc89_has_node(data);
    if (have_data)
    {
        st = jrpc89_set_data(a, err, data);
        if (st != JRPC89_OK)
        {
            return st;
        }
    }
    return JRPC89_OK;
}

jrpc89_status jrpc89_response_result_new(j89_arena *a, const jrpc89_id *id,
                                         j89_len result, j89_len *out)
{
    j89_len obj;
    jrpc89_status st;
    int ok;
    st = jrpc89_response_args_check(a, id, out);
    if (st != JRPC89_OK)
    {
        return st;
    }
    ok = jrpc89_has_node(result);
    if (!ok)
    {
        return JRPC89_EINVAL;
    }
    obj = j89_object_new(a, 3);
    ok = jrpc89_has_node(obj);
    if (!ok)
    {
        st = jrpc89_status_from_arena(a);
        return st;
    }
    st = jrpc89_set_version(a, obj, 0);
    if (st != JRPC89_OK)
    {
        return st;
    }
    j89_object_set(a, obj, 1, "result", 6, result);
    st = jrpc89_builder_check(a);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_set_id_member(a, obj, 2, id);
    if (st != JRPC89_OK)
    {
        return st;
    }
    *out = obj;
    return JRPC89_OK;
}

jrpc89_status jrpc89_response_error_new(j89_arena *a, const jrpc89_id *id,
                                        j89_int code, const char *message,
                                        j89_len message_len, j89_len data,
                                        j89_len *out)
{
    j89_len obj;
    j89_len err;
    j89_len count;
    jrpc89_status st;
    int exact;
    int ok;
    st = jrpc89_response_args_check(a, id, out);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_error_message_check(message, message_len);
    if (st != JRPC89_OK)
    {
        return st;
    }
    exact = jrpc89_int_exact(code);
    if (!exact)
    {
        return JRPC89_EINVAL;
    }
    if (message == NULL)
    {
        message = "";
    }
    count = jrpc89_error_member_count(data);
    obj = j89_object_new(a, 3);
    ok = jrpc89_has_node(obj);
    if (!ok)
    {
        st = jrpc89_status_from_arena(a);
        return st;
    }
    st = jrpc89_set_version(a, obj, 0);
    if (st != JRPC89_OK)
    {
        return st;
    }
    err = j89_object_new(a, count);
    ok = jrpc89_has_node(err);
    if (!ok)
    {
        st = jrpc89_status_from_arena(a);
        return st;
    }
    st = jrpc89_fill_error(a, err, code, message, message_len, data);
    if (st != JRPC89_OK)
    {
        return st;
    }
    j89_object_set(a, obj, 1, "error", 5, err);
    st = jrpc89_builder_check(a);
    if (st != JRPC89_OK)
    {
        return st;
    }
    st = jrpc89_set_id_member(a, obj, 2, id);
    if (st != JRPC89_OK)
    {
        return st;
    }
    *out = obj;
    return JRPC89_OK;
}
