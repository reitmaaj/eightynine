/* response.c - parse and validate JSON-RPC 2.0 response objects. */
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

/* Validate the error object of a response: it must be an object with an
 * integer code and a string message. Records a message and returns nonzero
 * when invalid. */
static int jrpc89_validate_error(j89_arena *a, j89_len resp)
{
    j89_kind ek;
    j89_kind ck;
    j89_kind mk;
    j89_len err;
    j89_len code;
    j89_len msg;
    int err_is_obj;
    int has_code;
    int has_msg;
    int code_ok;
    int msg_ok;
    err = j89_object_find(a, resp, "error");
    ek = j89_kind_of(a, err);
    err_is_obj = (ek == J89_OBJECT);
    if (!err_is_obj)
    {
        jrpc89_set_error(a, "jrpc89: response error is not an object");
        return -1;
    }
    code = j89_object_find(a, err, "code");
    has_code = jrpc89_has_node(code);
    if (!has_code)
    {
        jrpc89_set_error(a, "jrpc89: response error has no code");
        return -1;
    }
    ck = j89_kind_of(a, code);
    code_ok = (ck == J89_INTEGER);
    if (!code_ok)
    {
        jrpc89_set_error(a, "jrpc89: response error code is not integer");
        return -1;
    }
    msg = j89_object_find(a, err, "message");
    has_msg = jrpc89_has_node(msg);
    if (!has_msg)
    {
        jrpc89_set_error(a, "jrpc89: response error has no message");
        return -1;
    }
    mk = j89_kind_of(a, msg);
    msg_ok = (mk == J89_STRING);
    if (!msg_ok)
    {
        jrpc89_set_error(a, "jrpc89: response error message is not string");
        return -1;
    }
    return 0;
}

int jrpc89_response_validate(j89_arena *a, j89_len resp)
{
    j89_kind k;
    j89_len version;
    int have_result;
    int have_error;
    int have_id;
    int is_obj;
    int version_ok;
    k = j89_kind_of(a, resp);
    is_obj = (k == J89_OBJECT);
    if (!is_obj)
    {
        jrpc89_set_error(a, "jrpc89: response is not an object");
        return -1;
    }
    version = j89_object_find(a, resp, "jsonrpc");
    version_ok = jrpc89_has_node(version);
    if (!version_ok)
    {
        jrpc89_set_error(a, "jrpc89: response has no jsonrpc version");
        return -1;
    }
    version_ok = jrpc89_str_is(a, version, "2.0", 3);
    if (!version_ok)
    {
        jrpc89_set_error(a, "jrpc89: response jsonrpc is not \"2.0\"");
        return -1;
    }
    have_result = jrpc89_member_has(a, resp, "result");
    have_error = jrpc89_member_has(a, resp, "error");
    have_id = jrpc89_member_has(a, resp, "id");
    if (have_error)
    {
        int eok;
        eok = jrpc89_validate_error(a, resp);
        if (eok != 0)
        {
            return -1;
        }
    }
    if (have_result)
    {
        if (have_error)
        {
            jrpc89_set_error(a, "jrpc89: response has both result and error");
            return -1;
        }
    }
    if (!have_result)
    {
        if (!have_error)
        {
            jrpc89_set_error(a,
                             "jrpc89: response has neither result nor error");
            return -1;
        }
    }
    if (!have_id)
    {
        jrpc89_set_error(a, "jrpc89: response has no id member");
        return -1;
    }
    return 0;
}

int jrpc89_is_error(j89_arena *a, j89_len resp)
{
    j89_len v;
    int r;
    v = j89_object_find(a, resp, "error");
    r = jrpc89_has_node(v);
    return r;
}

j89_len jrpc89_result_node(j89_arena *a, j89_len resp)
{
    j89_len v;
    v = j89_object_find(a, resp, "result");
    return v;
}

static void jrpc89_set_id_int(j89_arena *a, j89_len node, jrpc89_id *out)
{
    j89_int num;
    num = j89_int_value(a, node);
    out->num = num;
    out->kind = JRPC89_ID_INT;
}

static void jrpc89_set_id_string(j89_arena *a, j89_len node, jrpc89_id *out)
{
    const char *str;
    j89_len len;
    str = j89_string_value(a, node);
    len = j89_string_length(a, node);
    out->str = str;
    out->len = len;
    out->kind = JRPC89_ID_STRING;
}

static void jrpc89_id_from_node(j89_arena *a, j89_len node, jrpc89_id *out)
{
    j89_kind k;
    k = j89_kind_of(a, node);
    if (k == J89_INTEGER)
    {
        jrpc89_set_id_int(a, node, out);
        return;
    }
    if (k == J89_STRING)
    {
        jrpc89_set_id_string(a, node, out);
        return;
    }
    out->kind = JRPC89_ID_NULL;
}

int jrpc89_id_of_response(j89_arena *a, j89_len resp, jrpc89_id *out)
{
    j89_len v;
    int has;
    v = j89_object_find(a, resp, "id");
    has = jrpc89_has_node(v);
    if (!has)
    {
        return -1;
    }
    jrpc89_id_from_node(a, v, out);
    return 0;
}
