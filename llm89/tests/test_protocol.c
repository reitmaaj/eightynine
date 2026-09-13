/* test_protocol.c - end-to-end tests against the local test server.
 *
 * Compile with the small resource limits used by the /big and /sse_big cases:
 *   -DLLM89_MAX_RESPONSE_BYTES=1024
 *   -DLLM89_MAX_SSE_EVENT_BYTES=256
 *   -DLLM89_MAX_ERROR_BODY_BYTES=512
 * Usage: test_protocol <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm89.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static int expect(int cond, const char *label)
{
    if (!cond)
    {
        fail(label);
        return 1;
    }
    return 0;
}

struct stream_collect
{
    char text[512];
    size_t len;
    int data_events;
    int done_events;
    int stop_at; /* nonzero: request stop after this many data events */
};

static int on_stream(void *userdata, const llm_stream_event *event)
{
    struct stream_collect *c;
    c = (struct stream_collect *)userdata;
    if (event->type == LLM_EVENT_DONE)
    {
        c->done_events = c->done_events + 1;
        return 0;
    }
    if (event->type == LLM_EVENT_DATA)
    {
        c->data_events = c->data_events + 1;
        if (event->text != NULL && event->text_len > 0)
        {
            size_t room;
            room = sizeof(c->text) - c->len - 1;
            if (event->text_len < room)
            {
                room = event->text_len;
            }
            memcpy(c->text + c->len, event->text, room);
            c->len = c->len + room;
            c->text[c->len] = '\0';
        }
        if (c->stop_at != 0 && c->data_events >= c->stop_at)
        {
            return 1;
        }
    }
    return 0;
}

static int on_cancel(void *userdata)
{
    (void)userdata;
    return 1;
}

static llm_client *make_client(const char *endpoint, const char *api_key,
                               llm_error *err)
{
    llm_client_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = endpoint;
    cfg.api_key = api_key;
    cfg.connect_timeout_ms = 2000L;
    cfg.timeout_ms = 0L;
    return llm_client_new(&cfg, err);
}

static void test_normal_json(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/ok", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "normal client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_OK, "normal rc");
        expect(resp.text != NULL && strcmp(resp.text, "hello") == 0,
               "normal text");
        expect(resp.json != NULL && strstr(resp.json, "hello") != NULL,
               "normal json");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_stream(int port, char *url, const char *path,
                        const char *want_text, int want_data)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d%s", port, path);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "stream client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        if (rc != LLM_OK)
        {
            fail("stream rc");
        }
        else
        {
            expect(c.data_events == want_data, "stream data events");
            expect(c.done_events == 1, "stream done event");
            expect(strcmp(c.text, want_text) == 0, "stream text");
        }
    }
    llm_client_free(client);
}

static void test_http_error(int port, char *url, const char *path, long status,
                            const char *expect_msg)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d%s", port, path);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "err client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EHTTP, "http error rc");
        expect(err.code == LLM_EHTTP, "http error rc code");
        expect(err.http_status == status, "http error status");
        expect(strstr(err.message, expect_msg) != NULL, "http error message");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_protocol_error(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/proto_err", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "proto client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EPROTO, "proto err rc");
        expect(err.code == LLM_EPROTO, "proto err rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_missing_done(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream_nodone", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "nodone client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        expect(rc == LLM_EPROTO, "missing done rc");
        expect(err.code == LLM_EPROTO, "missing done rc code");
    }
    llm_client_free(client);
}

static void test_badjson_stream(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream_badjson", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "badjson client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        expect(rc == LLM_OK, "badjson rc");
        expect(c.done_events == 1, "badjson done");
        expect(c.data_events == 1, "badjson data");
        expect(c.len == 0, "badjson no text");
    }
    llm_client_free(client);
}

static void test_connect_failure(char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:1/ok");
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "connect client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_ECURL, "connect failure rc");
        expect(err.code == LLM_ECURL, "connect failure rc code");
        expect(err.http_status == 0, "connect failure status");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_timeout(int port, char *url)
{
    llm_client_config cfg;
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/slow", port);
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = url;
    cfg.api_key = "secret";
    cfg.connect_timeout_ms = 2000L;
    cfg.timeout_ms = 200L;
    client = llm_client_new(&cfg, &err);
    if (expect(client != NULL, "timeout client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_ECURL, "timeout rc");
        expect(err.code == LLM_ECURL, "timeout rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_stream_cancel(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "scancel client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        c.stop_at = 1;
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        expect(rc == LLM_ECANCELLED, "stream cancel rc");
        expect(err.code == LLM_ECANCELLED, "stream cancel rc code");
        expect(c.data_events == 1, "stream cancel partial");
    }
    llm_client_free(client);
}

static void test_progress_cancel(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream_slow", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "pcancel client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, on_cancel, NULL, &err);
        expect(rc == LLM_ECANCELLED, "progress cancel rc");
        expect(err.code == LLM_ECANCELLED, "progress cancel rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_custom_header(int port, char *url)
{
    llm_client_config cfg;
    llm_header hdrs[1];
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/echo_hdr", port);
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = url;
    cfg.api_key = "secret";
    hdrs[0].name = "X-Test";
    hdrs[0].value = "hello";
    cfg.headers = hdrs;
    cfg.header_count = 1;
    cfg.connect_timeout_ms = 2000L;
    cfg.timeout_ms = 0L;
    client = llm_client_new(&cfg, &err);
    if (expect(client != NULL, "hdr client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_OK, "custom header rc");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_no_api_key(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/noauth", port);
    client = make_client(url, NULL, &err);
    if (expect(client != NULL, "noauth client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_OK, "no api key rc");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_response_limit(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/big", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "big client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EOVERFLOW, "response limit rc");
        expect(err.code == LLM_EOVERFLOW, "response limit rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_sse_limit(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/sse_big", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "ssebig client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        expect(rc == LLM_EOVERFLOW, "sse limit rc");
        expect(err.code == LLM_EOVERFLOW, "sse limit rc code");
    }
    llm_client_free(client);
}

static void test_dup_header(void)
{
    llm_client_config cfg;
    llm_header hdrs[1];
    llm_error err;
    llm_client *client;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = "http://127.0.0.1:1/ok";
    cfg.api_key = "secret";
    hdrs[0].name = "Authorization";
    hdrs[0].value = "Bearer x";
    cfg.headers = hdrs;
    cfg.header_count = 1;
    client = llm_client_new(&cfg, &err);
    expect(client == NULL, "dup header rejected");
    expect(err.code == LLM_EINVAL, "dup header code");
    llm_client_free(client);
}

static void test_raw_json_bad(void)
{
    llm_client *client;
    llm_response resp;
    llm_error err;
    int rc;
    client = make_client("http://127.0.0.1:1/ok", "secret", &err);
    if (expect(client != NULL, "rawbad client") == 0 && client != NULL)
    {
        memset(&resp, 0, sizeof(resp));
        rc = llm_json(client, "[1,2]", &resp, NULL, NULL, &err);
        expect(rc == LLM_EJSON, "raw non-object rc");
        expect(err.code == LLM_EJSON, "raw non-object rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_raw_json_ok(int port, char *url)
{
    llm_client *client;
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/ok", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "rawok client") == 0 && client != NULL)
    {
        memset(&resp, 0, sizeof(resp));
        rc = llm_json(client, "{\"model\":\"m\"}", &resp, NULL, NULL, &err);
        expect(rc == LLM_OK, "raw ok rc");
        expect(resp.json != NULL, "raw ok json");
        expect(resp.text == NULL, "raw ok text null");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_invalid_input(void)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    client = make_client("http://127.0.0.1:1/ok", "secret", &err);
    if (expect(client != NULL, "invalid client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = 99;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EINVAL, "invalid role rc");
        expect(err.code == LLM_EINVAL, "invalid role rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_badjson_response(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/badjson", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "badjson client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EJSON, "badjson rc is EJSON");
        expect(err.code == LLM_EJSON, "badjson rc is EJSON code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_stream_http_error(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/err_msg", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "stremerr client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&c, 0, sizeof(c));
        rc = llm_chat_stream(client, &req, on_stream, &c, NULL, NULL, &err);
        expect(rc == LLM_EHTTP, "stream http error rc");
        expect(err.code == LLM_EHTTP, "stream http error rc code");
        expect(err.http_status == 400, "stream http error status");
        expect(strstr(err.message, "the server said no") != NULL,
               "stream http error message");
        expect(c.data_events == 0, "stream http error no data");
        expect(c.done_events == 0, "stream http error no done");
    }
    llm_client_free(client);
}

static void test_null_header(void)
{
    llm_client_config cfg;
    llm_header hdrs[2];
    llm_error err;
    llm_client *client;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = "http://127.0.0.1:1/ok";
    cfg.api_key = "secret";
    memset(hdrs, 0, sizeof(hdrs));
    hdrs[0].name = NULL;
    hdrs[0].value = "v";
    cfg.headers = hdrs;
    cfg.header_count = 1;
    client = llm_client_new(&cfg, &err);
    expect(client == NULL, "null header name rejected");
    expect(err.code == LLM_EINVAL, "null header name code");
    llm_client_free(client);

    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint = "http://127.0.0.1:1/ok";
    cfg.api_key = "secret";
    hdrs[0].name = "X-Test";
    hdrs[0].value = NULL;
    cfg.headers = hdrs;
    cfg.header_count = 1;
    client = llm_client_new(&cfg, &err);
    expect(client == NULL, "null header value rejected");
    expect(err.code == LLM_EINVAL, "null header value code");
    llm_client_free(client);
}

static void test_nonstream_gets_sse(int port, char *url)
{
    llm_client *client;
    llm_chat_request req;
    llm_message msgs[1];
    llm_response resp;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "nonstream-sse client") == 0 && client != NULL)
    {
        memset(&req, 0, sizeof(req));
        memset(&msgs, 0, sizeof(msgs));
        msgs[0].role = LLM_ROLE_USER;
        msgs[0].content = "hi";
        req.model = "m";
        req.messages = msgs;
        req.message_count = 1;
        memset(&resp, 0, sizeof(resp));
        rc = llm_chat(client, &req, &resp, NULL, NULL, &err);
        expect(rc == LLM_EJSON, "nonstream-sse rc");
        expect(err.code == LLM_EJSON, "nonstream-sse rc code");
        llm_response_free(&resp);
    }
    llm_client_free(client);
}

static void test_llm_json_stream(int port, char *url)
{
    llm_client *client;
    struct stream_collect c;
    llm_error err;
    int rc;
    sprintf(url, "http://127.0.0.1:%d/stream", port);
    client = make_client(url, "secret", &err);
    if (expect(client != NULL, "rawstream client") == 0 && client != NULL)
    {
        memset(&c, 0, sizeof(c));
        rc = llm_json_stream(client, "{\"stream\":true}", on_stream, &c, NULL,
                             NULL, &err);
        expect(rc == LLM_OK, "raw stream rc");
        expect(c.done_events == 1, "raw stream done");
        expect(c.len == 0, "raw stream no text");
    }
    llm_client_free(client);
}

int main(int argc, char **argv)
{
    llm_error err;
    int port;
    char url[128];
    if (argc < 2)
    {
        fprintf(stderr, "usage: test_protocol <port>\n");
        return 2;
    }
    port = atoi(argv[1]);
    failures = 0;
    if (llm_global_init(&err) != LLM_OK)
    {
        fprintf(stderr, "global init failed: %s\n", err.message);
        return 2;
    }
    test_normal_json(port, url);
    test_stream(port, url, "/stream", "Hello world", 2);
    test_stream(port, url, "/stream_chunked", "Hello world", 2);
    test_http_error(port, url, "/err/400", 400, "400");
    test_http_error(port, url, "/err/401", 401, "401");
    test_http_error(port, url, "/err/429", 429, "429");
    test_http_error(port, url, "/err/500", 500, "500");
    test_http_error(port, url, "/err_nonjson", 500, "500");
    test_protocol_error(port, url);
    test_badjson_response(port, url);
    test_missing_done(port, url);
    test_badjson_stream(port, url);
    test_stream_http_error(port, url);
    test_connect_failure(url);
    test_timeout(port, url);
    test_stream_cancel(port, url);
    test_progress_cancel(port, url);
    test_custom_header(port, url);
    test_no_api_key(port, url);
    test_response_limit(port, url);
    test_sse_limit(port, url);
    test_dup_header();
    test_null_header();
    test_raw_json_bad();
    test_raw_json_ok(port, url);
    test_nonstream_gets_sse(port, url);
    test_invalid_input();
    test_llm_json_stream(port, url);
    llm_global_cleanup();
    if (failures != 0)
    {
        fprintf(stderr, "test_protocol: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_protocol: OK\n");
    return 0;
}
