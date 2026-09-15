/* error.c - JSON-RPC 2.0 error codes and error-object access. */
#include <jrpc89.h>

#include "jrpc89_internal.h"

int jrpc89_error_is_reserved(j89_int code)
{
    int r;
    r = 0;
    if (code >= -32768.0)
    {
        if (code <= -32000.0)
        {
            r = 1;
        }
    }
    return r;
}

j89_len jrpc89_error_object(j89_arena *a, j89_len resp)
{
    j89_len v;
    v = j89_object_find(a, resp, "error");
    return v;
}

j89_int jrpc89_error_code(j89_arena *a, j89_len resp)
{
    j89_len err;
    j89_len code;
    j89_int v;
    int has_err;
    int has_code;
    err = jrpc89_error_object(a, resp);
    has_err = jrpc89_has_node(err);
    if (!has_err)
    {
        return 0;
    }
    code = j89_object_find(a, err, "code");
    has_code = jrpc89_has_node(code);
    if (!has_code)
    {
        return 0;
    }
    v = j89_int_value(a, code);
    return v;
}

const char *jrpc89_error_message(j89_arena *a, j89_len resp)
{
    j89_len err;
    j89_len msg;
    const char *v;
    int has_err;
    int has_msg;
    err = jrpc89_error_object(a, resp);
    has_err = jrpc89_has_node(err);
    if (!has_err)
    {
        return "";
    }
    msg = j89_object_find(a, err, "message");
    has_msg = jrpc89_has_node(msg);
    if (!has_msg)
    {
        return "";
    }
    v = j89_string_value(a, msg);
    return v;
}

j89_len jrpc89_error_message_length(j89_arena *a, j89_len resp)
{
    j89_len err;
    j89_len msg;
    j89_len v;
    int has_err;
    int has_msg;
    err = jrpc89_error_object(a, resp);
    has_err = jrpc89_has_node(err);
    if (!has_err)
    {
        return 0;
    }
    msg = j89_object_find(a, err, "message");
    has_msg = jrpc89_has_node(msg);
    if (!has_msg)
    {
        return 0;
    }
    v = j89_string_length(a, msg);
    return v;
}

j89_len jrpc89_error_data(j89_arena *a, j89_len resp)
{
    j89_len err;
    j89_len v;
    int has_err;
    err = jrpc89_error_object(a, resp);
    has_err = jrpc89_has_node(err);
    if (!has_err)
    {
        return J89_BAD;
    }
    v = j89_object_find(a, err, "data");
    return v;
}
