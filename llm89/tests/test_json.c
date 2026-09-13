/* test_json.c - unit tests for the libj89-backed JSON module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j89.h"
#include "llm89.h"
#include "../src/llm89_json.h"

#ifndef INFINITY
#define INFINITY (1.0 / 0.0)
#endif

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static int has_substring(const char *hay, const char *needle)
{
    if (hay == NULL)
    {
        return 0;
    }
    return strstr(hay, needle) != NULL;
}

static void test_build_minimal(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_USER;
    msgs[0].content = "hi";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_OK || out == NULL)
    {
        fail("build minimal rc");
        return;
    }
    if (!has_substring(out, "\"model\":\"m\""))
    {
        fail("build model");
    }
    if (!has_substring(out, "\"role\":\"user\""))
    {
        fail("build role");
    }
    if (!has_substring(out, "\"content\":\"hi\""))
    {
        fail("build content");
    }
    if (!has_substring(out, "\"stream\":false"))
    {
        fail("build stream false");
    }
    if (has_substring(out, "temperature"))
    {
        fail("build no temperature");
    }
    if (has_substring(out, "max_tokens"))
    {
        fail("build no max_tokens");
    }
    free(out);
}

static void test_build_stream_true(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_USER;
    msgs[0].content = "x";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    rc = llm_json_build_request(&req, 1, &out);
    if (rc != LLM_OK || out == NULL)
    {
        fail("build stream rc");
        return;
    }
    if (!has_substring(out, "\"stream\":true"))
    {
        fail("build stream true");
    }
    free(out);
}

static void test_build_optional_fields(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    j89_arena a;
    j89_len root;
    j89_len t;
    j89_len m;
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_SYSTEM;
    msgs[0].content = "sys";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    req.set_temperature = 1;
    req.temperature = 0.7;
    req.set_max_tokens = 1;
    req.max_tokens = 64;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_OK || out == NULL)
    {
        fail("build optional rc");
        return;
    }
    if (!has_substring(out, "\"role\":\"system\""))
    {
        fail("build system role");
    }
    j89_arena_init(&a);
    root = j89_parse(out, (j89_len)strlen(out), &a);
    if (root == J89_BAD)
    {
        fail("build optional parse");
    }
    else
    {
        t = j89_object_find(&a, root, "temperature");
        m = j89_object_find(&a, root, "max_tokens");
        if (j89_kind_of(&a, t) != J89_FLOAT || j89_double_value(&a, t) != 0.7)
        {
            fail("build temperature value");
        }
        if (j89_kind_of(&a, m) != J89_INTEGER || j89_int_value(&a, m) != 64)
        {
            fail("build max_tokens value");
        }
    }
    j89_arena_destroy(&a);
    free(out);
}

static void test_build_escaping(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_ASSISTANT;
    msgs[0].content = "say \"hi\" and \\ done\nnext";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_OK || out == NULL)
    {
        fail("build escaping rc");
        return;
    }
    if (!has_substring(out, "\"role\":\"assistant\""))
    {
        fail("build assistant role");
    }
    if (!has_substring(out, "say \\\"hi\\\" and \\\\ done\\nnext"))
    {
        fail("build escaped content");
    }
    free(out);
}

static void test_build_roundtrip_multiple(void)
{
    llm_chat_request req;
    llm_message msgs[2];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_SYSTEM;
    msgs[0].content = "a";
    msgs[1].role = LLM_ROLE_USER;
    msgs[1].content = "b";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 2;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_OK || out == NULL)
    {
        fail("build multi rc");
        return;
    }
    if (!has_substring(out, "\"a\""))
    {
        fail("build first content");
    }
    if (!has_substring(out, "\"b\""))
    {
        fail("build second content");
    }
    free(out);
}

static void test_build_invalid_role(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = 99;
    msgs[0].content = "x";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("build invalid role code");
    }
    if (out != NULL)
    {
        fail("build invalid role out");
        free(out);
    }
}

static void test_build_missing_model(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_USER;
    msgs[0].content = "x";
    req.model = "";
    req.messages = msgs;
    req.message_count = 1;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("build empty model code");
    }
}

static void test_build_zero_messages(void)
{
    llm_chat_request req;
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    req.model = "m";
    req.message_count = 0;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("build zero messages code");
    }
}

static void test_build_negative_max_tokens(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_USER;
    msgs[0].content = "x";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    req.set_max_tokens = 1;
    req.max_tokens = -1;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("build negative max_tokens code");
    }
}

static void test_extract_content(void)
{
    static const char body[] =
        "{\"choices\":[{\"message\":{\"content\":\"hello\"}}]}";
    char *out;
    out = llm_json_extract_content(body, sizeof(body) - 1);
    if (out == NULL)
    {
        fail("extract content null");
        return;
    }
    if (strcmp(out, "hello") != 0)
    {
        fail("extract content value");
    }
    free(out);
}

static void test_extract_content_missing(void)
{
    static const char body[] = "{\"choices\":[]}";
    char *out;
    out = llm_json_extract_content(body, sizeof(body) - 1);
    if (out != NULL)
    {
        fail("extract content empty choices");
        free(out);
    }
}

static void test_extract_content_no_choices(void)
{
    static const char body[] = "{\"foo\":1}";
    char *out;
    out = llm_json_extract_content(body, sizeof(body) - 1);
    if (out != NULL)
    {
        fail("extract content no choices");
        free(out);
    }
}

static void test_extract_delta(void)
{
    static const char body[] =
        "{\"choices\":[{\"delta\":{\"content\":\"hi\"}}]}";
    char *out;
    out = llm_json_extract_delta(body, sizeof(body) - 1);
    if (out == NULL)
    {
        fail("extract delta null");
        return;
    }
    if (strcmp(out, "hi") != 0)
    {
        fail("extract delta value");
    }
    free(out);
}

static void test_extract_delta_no_content(void)
{
    static const char body[] = "{\"choices\":[{\"delta\":{}}]}";
    char *out;
    out = llm_json_extract_delta(body, sizeof(body) - 1);
    if (out != NULL)
    {
        fail("extract delta empty");
        free(out);
    }
}

static void test_validate_object(void)
{
    if (llm_json_validate_object("{\"a\":1}", 7) != 0)
    {
        fail("validate object ok");
    }
    if (llm_json_validate_object("[1,2]", 5) == 0)
    {
        fail("validate array rejected");
    }
    if (llm_json_validate_object("42", 2) == 0)
    {
        fail("validate number rejected");
    }
    if (llm_json_validate_object("not json", 8) == 0)
    {
        fail("validate garbage rejected");
    }
}

static void test_extract_error_message(void)
{
    static const char body[] = "{\"error\":{\"message\":\"bad key\"}}";
    char *out;
    out = llm_json_extract_error_message(body, sizeof(body) - 1);
    if (out == NULL)
    {
        fail("extract error null");
        return;
    }
    if (strcmp(out, "bad key") != 0)
    {
        fail("extract error value");
    }
    free(out);
}

static void test_extract_error_message_missing(void)
{
    static const char body[] = "{\"other\":1}";
    char *out;
    out = llm_json_extract_error_message(body, sizeof(body) - 1);
    if (out != NULL)
    {
        fail("extract error missing");
        free(out);
    }
}

static void test_parses(void)
{
    if (!llm_json_parses("{\"a\":1}", 7))
    {
        fail("parses object");
    }
    if (!llm_json_parses("[1,2]", 5))
    {
        fail("parses array");
    }
    if (!llm_json_parses("42", 2))
    {
        fail("parses number");
    }
    if (llm_json_parses("{invalid", 8))
    {
        fail("reject invalid");
    }
    if (llm_json_parses("", 0))
    {
        fail("reject empty");
    }
}

static void test_build_nonfinite_temperature(void)
{
    llm_chat_request req;
    llm_message msgs[1];
    double nan_val;
    char *out;
    int rc;
    memset(&req, 0, sizeof(req));
    memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = LLM_ROLE_USER;
    msgs[0].content = "x";
    req.model = "m";
    req.messages = msgs;
    req.message_count = 1;
    req.set_temperature = 1;
    nan_val = 0.0;
    nan_val = nan_val / nan_val;
    req.temperature = nan_val;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("nonfinite NaN temperature code");
    }
    if (out != NULL)
    {
        free(out);
    }
    req.temperature = INFINITY;
    out = NULL;
    rc = llm_json_build_request(&req, 0, &out);
    if (rc != LLM_EINVAL)
    {
        fail("nonfinite inf temperature code");
    }
    if (out != NULL)
    {
        free(out);
    }
}

int main(void)
{
    failures = 0;
    test_build_minimal();
    test_build_stream_true();
    test_build_optional_fields();
    test_build_escaping();
    test_build_roundtrip_multiple();
    test_build_invalid_role();
    test_build_missing_model();
    test_build_zero_messages();
    test_build_negative_max_tokens();
    test_build_nonfinite_temperature();
    test_extract_content();
    test_extract_content_missing();
    test_extract_content_no_choices();
    test_extract_delta();
    test_extract_delta_no_content();
    test_validate_object();
    test_parses();
    test_extract_error_message();
    test_extract_error_message_missing();
    if (failures != 0)
    {
        fprintf(stderr, "test_json: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_json: OK\n");
    return 0;
}
