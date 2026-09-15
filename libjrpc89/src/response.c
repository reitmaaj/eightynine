/* response.c - decode JSON-RPC 2.0 response objects. */
#include <jrpc89.h>

#include "jrpc89_internal.h"

static int jrpc89_str_is(j89_arena *a, j89_len node, const char *expect,
                         j89_len expect_len)
{
    j89_kind k;
    j89_len len;
    const char *s;
    j89_len i;
    k = j89_kind_of(a, node);
    if (k != J89_STRING)
    {
        return 0;
    }
    len = j89_string_length(a, node);
    if (len != expect_len)
    {
        return 0;
    }
    s = j89_string_value(a, node);
    for (i = 0; i < len; i = i + 1)
    {
        if (s[i] != expect[i])
        {
            return 0;
        }
    }
    return 1;
}

static int jrpc89_member_has(j89_arena *a, j89_len node, const char *key)
{
    j89_len v;
    int r;
    v = j89_object_find(a, node, key);
    r = jrpc89_has_node(v);
    return r;
}

static void jrpc89_id_set_int(j89_arena *a, j89_len node, jrpc89_id *out)
{
    out->kind = JRPC89_ID_INT;
    out->num = j89_int_value(a, node);
}

static void jrpc89_id_set_string(j89_arena *a, j89_len node, jrpc89_id *out)
{
    out->kind = JRPC89_ID_STRING;
    out->str = j89_string_value(a, node);
    out->len = j89_string_length(a, node);
}

/* Convert one node to an id. Returns JRPC89_OK for integer, string, or
 * null, and JRPC89_EPROTO for every other node kind. */
static jrpc89_status jrpc89_id_from_node(j89_arena *a, j89_len node,
                                         jrpc89_id *out)
{
    j89_kind k;
    k = j89_kind_of(a, node);
    if (k == J89_INTEGER)
    {
        jrpc89_id_set_int(a, node, out);
        return JRPC89_OK;
    }
    if (k == J89_STRING)
    {
        jrpc89_id_set_string(a, node, out);
        return JRPC89_OK;
    }
    if (k == J89_NULL)
    {
        out->kind = JRPC89_ID_NULL;
        return JRPC89_OK;
    }
    return JRPC89_EPROTO;
}

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

/* Check the fixed parts of a response: object root and a "2.0" version. */
static jrpc89_status jrpc89_check_head(j89_arena *a, j89_len node)
{
    j89_len version;
    j89_kind k;
    int has;
    int ok;
    k = j89_kind_of(a, node);
    if (k != J89_OBJECT)
    {
        return JRPC89_EPROTO;
    }
    version = j89_object_find(a, node, "jsonrpc");
    has = jrpc89_has_node(version);
    if (!has)
    {
        return JRPC89_EPROTO;
    }
    ok = jrpc89_str_is(a, version, "2.0", 3);
    if (!ok)
    {
        return JRPC89_EPROTO;
    }
    return JRPC89_OK;
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
