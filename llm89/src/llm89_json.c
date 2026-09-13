/* llm89_json.c - request construction and response extraction via libj89. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "j89.h"
#include "llm89_json.h"

static char *arena_to_cstr(const j89_arena *out)
{
    j89_len n;
    char *s;
    n = out->off;
    s = (char *)malloc((size_t)n + 1);
    if (s == NULL)
    {
        return NULL;
    }
    if (n != 0)
    {
        memcpy(s, out->mem, (size_t)n);
    }
    s[n] = '\0';
    return s;
}

static int valid_role(int role)
{
    int ok;
    ok = (role == LLM_ROLE_SYSTEM) || (role == LLM_ROLE_USER) ||
         (role == LLM_ROLE_ASSISTANT);
    return ok;
}

static const char *role_name(int role)
{
    if (role == LLM_ROLE_SYSTEM)
    {
        return "system";
    }
    if (role == LLM_ROLE_USER)
    {
        return "user";
    }
    return "assistant";
}

static int temperature_nonfinite(double v)
{
    int nan;
    int pinf;
    int ninf;
    nan = (v != v);
    pinf = (v == HUGE_VAL);
    ninf = (v == -HUGE_VAL);
    return nan || pinf || ninf;
}

int llm_json_build_request(const llm_chat_request *req, int streaming,
                           char **out)
{
    j89_arena a;
    j89_arena res;
    j89_len root;
    j89_len arr;
    size_t i;
    int nfields;
    int idx;
    int rc;
    *out = NULL;
    if (req == NULL)
    {
        return LLM_EINVAL;
    }
    if (req->model == NULL || req->model[0] == '\0')
    {
        return LLM_EINVAL;
    }
    if (req->message_count == 0 || req->messages == NULL)
    {
        return LLM_EINVAL;
    }
    i = 0;
    while (i < req->message_count)
    {
        if (req->messages[i].content == NULL)
        {
            return LLM_EINVAL;
        }
        if (!valid_role(req->messages[i].role))
        {
            return LLM_EINVAL;
        }
        i = i + 1;
    }
    if (req->set_max_tokens && req->max_tokens < 0)
    {
        return LLM_EINVAL;
    }
    if (req->set_temperature && temperature_nonfinite(req->temperature))
    {
        return LLM_EINVAL;
    }
    nfields = 3;
    if (req->set_temperature)
    {
        nfields = nfields + 1;
    }
    if (req->set_max_tokens)
    {
        nfields = nfields + 1;
    }
    j89_arena_init(&a);
    j89_arena_init(&res);
    root = j89_object_new(&a, (j89_len)nfields);
    arr = j89_array_new(&a, req->message_count);
    j89_object_set(&a, root, 0, "model", 5,
                   j89_string_new(&a, req->model, (j89_len)strlen(req->model)));
    j89_object_set(&a, root, 1, "messages", 8, arr);
    if (streaming)
    {
        j89_object_set(&a, root, 2, "stream", 6, j89_bool_new(&a, 1));
    }
    else
    {
        j89_object_set(&a, root, 2, "stream", 6, j89_bool_new(&a, 0));
    }
    idx = 3;
    if (req->set_temperature)
    {
        j89_object_set(&a, root, (j89_len)idx, "temperature", 11,
                       j89_double_new(&a, req->temperature));
        idx = idx + 1;
    }
    if (req->set_max_tokens)
    {
        j89_object_set(&a, root, (j89_len)idx, "max_tokens", 10,
                       j89_integer_new(&a, (j89_int)req->max_tokens));
        idx = idx + 1;
    }
    for (i = 0; i < req->message_count; i = i + 1)
    {
        j89_len msg;
        j89_len content;
        const char *rn;
        j89_len rl;
        rn = role_name(req->messages[i].role);
        rl = (j89_len)strlen(rn);
        msg = j89_object_new(&a, 2);
        content = j89_string_new(&a, req->messages[i].content,
                                 (j89_len)strlen(req->messages[i].content));
        j89_object_set(&a, msg, 0, "role", 4, j89_string_new(&a, rn, rl));
        j89_object_set(&a, msg, 1, "content", 7, content);
        j89_array_set(&a, arr, i, msg);
    }
    rc = j89_render(&a, root, 1, &res);
    if (rc != 0 || res.failed)
    {
        rc = LLM_EJSON;
        j89_arena_destroy(&res);
        j89_arena_destroy(&a);
        return rc;
    }
    *out = arena_to_cstr(&res);
    j89_arena_destroy(&res);
    j89_arena_destroy(&a);
    if (*out == NULL)
    {
        return LLM_ENOMEM;
    }
    return LLM_OK;
}

static char *extract_string_path(const char *json, size_t len,
                                 const char *child)
{
    j89_arena a;
    j89_len root;
    j89_len choices;
    j89_len first;
    j89_len obj;
    j89_len content;
    char *res;
    j89_arena_init(&a);
    root = j89_parse(json, (j89_len)len, &a);
    if (root == J89_BAD || j89_kind_of(&a, root) != J89_OBJECT)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    choices = j89_object_find(&a, root, "choices");
    if (choices == J89_BAD || j89_kind_of(&a, choices) != J89_ARRAY ||
        j89_array_length(&a, choices) == 0)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    first = j89_array_get(&a, choices, 0);
    if (j89_kind_of(&a, first) != J89_OBJECT)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    obj = j89_object_find(&a, first, child);
    if (obj == J89_BAD || j89_kind_of(&a, obj) != J89_OBJECT)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    content = j89_object_find(&a, obj, "content");
    if (content == J89_BAD || j89_kind_of(&a, content) != J89_STRING)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    res = (char *)malloc((size_t)j89_string_length(&a, content) + 1);
    if (res == NULL)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    strcpy(res, j89_string_value(&a, content));
    j89_arena_destroy(&a);
    return res;
}

char *llm_json_extract_content(const char *json, size_t len)
{
    return extract_string_path(json, len, "message");
}

char *llm_json_extract_delta(const char *json, size_t len)
{
    return extract_string_path(json, len, "delta");
}

int llm_json_validate_object(const char *json, size_t len)
{
    j89_arena a;
    j89_len root;
    int ok;
    j89_arena_init(&a);
    root = j89_parse(json, (j89_len)len, &a);
    ok = (root != J89_BAD && j89_kind_of(&a, root) == J89_OBJECT);
    j89_arena_destroy(&a);
    if (ok)
    {
        return 0;
    }
    return 1;
}

int llm_json_parses(const char *json, size_t len)
{
    j89_arena a;
    j89_len root;
    int ok;
    j89_arena_init(&a);
    root = j89_parse(json, (j89_len)len, &a);
    ok = (root != J89_BAD);
    j89_arena_destroy(&a);
    return ok;
}

char *llm_json_extract_error_message(const char *json, size_t len)
{
    j89_arena a;
    j89_len root;
    j89_len err;
    j89_len msg;
    char *res;
    j89_arena_init(&a);
    root = j89_parse(json, (j89_len)len, &a);
    if (root == J89_BAD || j89_kind_of(&a, root) != J89_OBJECT)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    err = j89_object_find(&a, root, "error");
    if (err == J89_BAD || j89_kind_of(&a, err) != J89_OBJECT)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    msg = j89_object_find(&a, err, "message");
    if (msg == J89_BAD || j89_kind_of(&a, msg) != J89_STRING)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    res = (char *)malloc((size_t)j89_string_length(&a, msg) + 1);
    if (res == NULL)
    {
        j89_arena_destroy(&a);
        return NULL;
    }
    strcpy(res, j89_string_value(&a, msg));
    j89_arena_destroy(&a);
    return res;
}
