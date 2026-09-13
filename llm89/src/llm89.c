/* llm89.c - public API and libcurl transport for llm89. */
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "llm89.h"
#include "llm89_sse.h"
#include "llm89_buf.h"
#include "llm89_json.h"

#ifndef LLM89_MAX_RESPONSE_BYTES
#define LLM89_MAX_RESPONSE_BYTES (16 * 1024 * 1024)
#endif
#ifndef LLM89_MAX_SSE_EVENT_BYTES
#define LLM89_MAX_SSE_EVENT_BYTES (4 * 1024 * 1024)
#endif
#ifndef LLM89_MAX_ERROR_BODY_BYTES
#define LLM89_MAX_ERROR_BODY_BYTES (1024 * 1024)
#endif

#define LLM89_DEFAULT_CONNECT_TIMEOUT_MS 10000L

struct llm_client
{
    char *endpoint;
    char *api_key;
    char **hdr_names;
    char **hdr_values;
    size_t header_count;
    long connect_timeout_ms;
    long timeout_ms;
    CURL *curl;
};

typedef struct llm_xfer
{
    struct llm_client *client;
    llm_buf body;
    llm_buf errbody;
    long status;
    int cancelled;
    int overflow;
    const char *req_data;
    size_t req_len;
    llm_cancel_fn cancel;
    void *cancel_userdata;
    int streaming;
    llm_sse sse;
    llm_stream_fn stream;
    void *stream_userdata;
    int extract_delta;
    int saw_done;
    char errbuf[CURL_ERROR_SIZE];
} llm_xfer;

static void clear_error(llm_error *error)
{
    if (error != NULL)
    {
        memset(error, 0, sizeof(*error));
    }
}

static void copy_message(llm_error *error, const char *s)
{
    size_t i;
    if (error == NULL)
    {
        return;
    }
    if (s == NULL)
    {
        s = "";
    }
    i = 0;
    while (s[i] != '\0' && i < sizeof(error->message) - 1)
    {
        error->message[i] = s[i];
        i = i + 1;
    }
    error->message[i] = '\0';
}

static void set_error_code(llm_error *error, int code)
{
    if (error != NULL)
    {
        error->code = code;
    }
}

static int err_ret(llm_error *error, int code)
{
    set_error_code(error, code);
    return code;
}

static int streq_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        char ca;
        char cb;
        ca = *a;
        cb = *b;
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb)
        {
            return 0;
        }
        a = a + 1;
        b = b + 1;
    }
    return (*a == '\0' && *b == '\0');
}

static char *dup_str(const char *s)
{
    size_t n;
    char *d;
    if (s == NULL)
    {
        return NULL;
    }
    n = strlen(s);
    d = (char *)malloc(n + 1);
    if (d == NULL)
    {
        return NULL;
    }
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

/* ---- transfer callbacks ---- */

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    llm_xfer *x;
    size_t n;
    x = (llm_xfer *)userdata;
    n = size * nmemb;
    if (x->cancelled)
    {
        return 0;
    }
    if (x->status >= 300)
    {
        if (llm_buf_append(&x->errbody, ptr, n) != 0)
        {
            x->overflow = 1;
            return 0;
        }
        return n;
    }
    if (x->streaming)
    {
        int r;
        r = llm_sse_feed(&x->sse, ptr, n);
        if (r == 1)
        {
            x->overflow = 1;
            return 0;
        }
        if (r == -1)
        {
            x->cancelled = 1;
            return 0;
        }
        return n;
    }
    if (llm_buf_append(&x->body, ptr, n) != 0)
    {
        x->overflow = 1;
        return 0;
    }
    return n;
}

static size_t header_cb(char *buffer, size_t size, size_t nitems,
                        void *userdata)
{
    llm_xfer *x;
    size_t n;
    size_t j;
    x = (llm_xfer *)userdata;
    n = size * nitems;
    if (x->status != 0)
    {
        return n;
    }
    if (n >= 5 && buffer[0] == 'H' && buffer[1] == 'T' && buffer[2] == 'T' &&
        buffer[3] == 'P' && buffer[4] == '/')
    {
        long code;
        j = 5;
        while (j < n && buffer[j] != ' ' && buffer[j] != '\r' &&
               buffer[j] != '\n')
        {
            j = j + 1;
        }
        while (j < n && buffer[j] == ' ')
        {
            j = j + 1;
        }
        if (j + 3 <= n && buffer[j] >= '0' && buffer[j] <= '9' &&
            buffer[j + 1] >= '0' && buffer[j + 1] <= '9' &&
            buffer[j + 2] >= '0' && buffer[j + 2] <= '9')
        {
            code = (long)((buffer[j] - '0') * 100 + (buffer[j + 1] - '0') * 10 +
                          (buffer[j + 2] - '0'));
            x->status = code;
        }
    }
    return n;
}

static int sse_emit(void *userdata, const char *payload, size_t len)
{
    llm_xfer *x;
    llm_stream_event ev;
    char *text;
    x = (llm_xfer *)userdata;
    memset(&ev, 0, sizeof(ev));
    text = NULL;
    if (len == 6 && memcmp(payload, "[DONE]", 6) == 0)
    {
        ev.type = LLM_EVENT_DONE;
        ev.json = payload;
        ev.json_len = len;
        x->saw_done = 1;
    }
    else
    {
        ev.type = LLM_EVENT_DATA;
        ev.json = payload;
        ev.json_len = len;
        if (x->extract_delta)
        {
            text = llm_json_extract_delta(payload, len);
            if (text != NULL)
            {
                ev.text = text;
                ev.text_len = strlen(text);
            }
        }
    }
    if (x->stream != NULL)
    {
        if (x->stream(x->stream_userdata, &ev) != 0)
        {
            free(text);
            return 1;
        }
    }
    free(text);
    return 0;
}

static int xferinfo(void *userdata, curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
    llm_xfer *x;
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;
    x = (llm_xfer *)userdata;
    if (x->cancel != NULL)
    {
        if (x->cancel(x->cancel_userdata) != 0)
        {
            x->cancelled = 1;
            return 1;
        }
    }
    return 0;
}

/* ---- header list ---- */

static int build_headers(llm_xfer *x, struct curl_slist **out)
{
    struct curl_slist *list;
    size_t i;
    list = NULL;
    list = curl_slist_append(list, "Content-Type: application/json");
    if (list == NULL)
    {
        return LLM_ENOMEM;
    }
    if (x->streaming)
    {
        list = curl_slist_append(list, "Accept: text/event-stream");
    }
    else
    {
        list = curl_slist_append(list, "Accept: application/json");
    }
    if (list == NULL)
    {
        curl_slist_free_all(list);
        return LLM_ENOMEM;
    }
    if (x->client->api_key != NULL)
    {
        size_t n;
        char *auth;
        n = strlen("Authorization: Bearer ") + strlen(x->client->api_key) + 1;
        auth = (char *)malloc(n);
        if (auth == NULL)
        {
            curl_slist_free_all(list);
            return LLM_ENOMEM;
        }
        sprintf(auth, "Authorization: Bearer %s", x->client->api_key);
        list = curl_slist_append(list, auth);
        free(auth);
        if (list == NULL)
        {
            curl_slist_free_all(list);
            return LLM_ENOMEM;
        }
    }
    for (i = 0; i < x->client->header_count; i = i + 1)
    {
        size_t n;
        char *h;
        n = strlen(x->client->hdr_names[i]) + strlen(x->client->hdr_values[i]) +
            3;
        h = (char *)malloc(n);
        if (h == NULL)
        {
            curl_slist_free_all(list);
            return LLM_ENOMEM;
        }
        sprintf(h, "%s: %s", x->client->hdr_names[i], x->client->hdr_values[i]);
        list = curl_slist_append(list, h);
        free(h);
        if (list == NULL)
        {
            curl_slist_free_all(list);
            return LLM_ENOMEM;
        }
    }
    *out = list;
    return LLM_OK;
}

/* ---- transfer ---- */

static int perform(llm_xfer *x, struct curl_slist *hdrs, llm_error *error)
{
    CURL *c;
    CURLcode cr;
    long code;
    c = x->client->curl;
    if ((curl_off_t)x->req_len < 0)
    {
        set_error_code(error, LLM_EOVERFLOW);
        return LLM_EOVERFLOW;
    }
    curl_easy_reset(c);
    curl_easy_setopt(c, CURLOPT_URL, x->client->endpoint);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, x->req_data);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)x->req_len);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, x);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, x);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    if (x->cancel != NULL)
    {
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xferinfo);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, x);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    }
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS,
                     x->client->connect_timeout_ms);
    if (x->client->timeout_ms > 0)
    {
        curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, x->client->timeout_ms);
    }
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, x->errbuf);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
    x->errbuf[0] = '\0';
    cr = curl_easy_perform(c);
    if (x->cancelled)
    {
        set_error_code(error, LLM_ECANCELLED);
        return LLM_ECANCELLED;
    }
    if (x->overflow)
    {
        set_error_code(error, LLM_EOVERFLOW);
        return LLM_EOVERFLOW;
    }
    code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    x->status = code;
    if (cr != CURLE_OK)
    {
        if (error != NULL)
        {
            error->code = LLM_ECURL;
            error->transport_code = (long)cr;
            error->http_status = code;
            if (x->errbuf[0] != '\0')
            {
                copy_message(error, x->errbuf);
            }
            else
            {
                copy_message(error, "curl transfer failed");
            }
        }
        return LLM_ECURL;
    }
    if (code >= 200 && code < 300)
    {
        return LLM_OK;
    }
    if (error != NULL)
    {
        char *m;
        error->code = LLM_EHTTP;
        error->http_status = code;
        error->transport_code = 0;
        m = llm_json_extract_error_message(llm_buf_data(&x->errbody),
                                           llm_buf_len(&x->errbody));
        if (m != NULL)
        {
            copy_message(error, m);
            free(m);
        }
        else
        {
            sprintf(error->message, "HTTP error %ld", code);
        }
    }
    return LLM_EHTTP;
}

static char *copy_buf(llm_buf *b)
{
    size_t n;
    char *s;
    n = llm_buf_len(b);
    s = (char *)malloc(n + 1);
    if (s == NULL)
    {
        return NULL;
    }
    if (n != 0)
    {
        memcpy(s, llm_buf_data(b), n);
    }
    s[n] = '\0';
    return s;
}

static int run(struct llm_client *client, const char *req_data, size_t req_len,
               llm_response *response, llm_stream_fn stream,
               void *stream_userdata, llm_cancel_fn cancel,
               void *cancel_userdata, llm_error *error, int streaming,
               int extract_content, int extract_delta)
{
    llm_xfer x;
    struct curl_slist *hdrs;
    int rc;
    memset(&x, 0, sizeof(x));
    x.client = client;
    x.req_data = req_data;
    x.req_len = req_len;
    x.cancel = cancel;
    x.cancel_userdata = cancel_userdata;
    x.streaming = streaming;
    x.stream = stream;
    x.stream_userdata = stream_userdata;
    x.extract_delta = extract_delta;
    llm_buf_init(&x.body, LLM89_MAX_RESPONSE_BYTES);
    llm_buf_init(&x.errbody, LLM89_MAX_ERROR_BODY_BYTES);
    if (streaming)
    {
        llm_sse_init(&x.sse, LLM89_MAX_SSE_EVENT_BYTES, &x, sse_emit);
    }
    hdrs = NULL;
    rc = build_headers(&x, &hdrs);
    if (rc != LLM_OK)
    {
        set_error_code(error, rc);
    }
    else
    {
        rc = perform(&x, hdrs, error);
    }
    if (rc == LLM_OK && response != NULL)
    {
        response->http_status = x.status;
        response->json = copy_buf(&x.body);
        if (response->json == NULL)
        {
            rc = LLM_ENOMEM;
        }
        else if (extract_content)
        {
            response->text = llm_json_extract_content(llm_buf_data(&x.body),
                                                      llm_buf_len(&x.body));
            if (response->text == NULL)
            {
                llm_response_free(response);
                if (llm_json_parses(llm_buf_data(&x.body),
                                    llm_buf_len(&x.body)))
                {
                    rc = LLM_EPROTO;
                }
                else
                {
                    rc = LLM_EJSON;
                }
            }
        }
    }
    if (rc == LLM_OK && streaming && !x.saw_done)
    {
        rc = LLM_EPROTO;
    }
    if (rc != LLM_OK)
    {
        set_error_code(error, rc);
    }
    if (hdrs != NULL)
    {
        curl_slist_free_all(hdrs);
    }
    if (streaming)
    {
        llm_sse_destroy(&x.sse);
    }
    llm_buf_destroy(&x.body);
    llm_buf_destroy(&x.errbody);
    return rc;
}

/* ---- public API ---- */

int llm_global_init(llm_error *error)
{
    clear_error(error);
#ifndef LLM89_EXTERNAL_CURL_GLOBALS
    {
        CURLcode rc;
        rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK)
        {
            if (error != NULL)
            {
                error->code = LLM_ECURL;
                error->transport_code = (long)rc;
                copy_message(error, "curl_global_init failed");
            }
            return LLM_ECURL;
        }
    }
#else
    (void)0;
#endif
    return LLM_OK;
}

void llm_global_cleanup(void)
{
#ifndef LLM89_EXTERNAL_CURL_GLOBALS
    curl_global_cleanup();
#endif
}

llm_client *llm_client_new(const llm_client_config *config, llm_error *error)
{
    llm_client *client;
    size_t i;
    clear_error(error);
    if (config == NULL || config->endpoint == NULL)
    {
        set_error_code(error, LLM_EINVAL);
        copy_message(error, "endpoint required");
        return NULL;
    }
    if (config->timeout_ms < 0)
    {
        set_error_code(error, LLM_EINVAL);
        copy_message(error, "negative timeout");
        return NULL;
    }
    if (config->headers != NULL && config->header_count > 0)
    {
        for (i = 0; i < config->header_count; i = i + 1)
        {
            if (config->headers[i].name == NULL ||
                config->headers[i].value == NULL)
            {
                set_error_code(error, LLM_EINVAL);
                copy_message(error, "NULL header name or value");
                return NULL;
            }
            if (streq_case(config->headers[i].name, "Authorization") ||
                streq_case(config->headers[i].name, "Content-Type"))
            {
                set_error_code(error, LLM_EINVAL);
                copy_message(error, "reserved header in custom list");
                return NULL;
            }
        }
    }
    client = (llm_client *)malloc(sizeof(*client));
    if (client == NULL)
    {
        set_error_code(error, LLM_ENOMEM);
        return NULL;
    }
    memset(client, 0, sizeof(*client));
    client->endpoint = dup_str(config->endpoint);
    client->api_key = dup_str(config->api_key);
    if (client->endpoint == NULL)
    {
        llm_client_free(client);
        set_error_code(error, LLM_ENOMEM);
        return NULL;
    }
    if (config->headers != NULL && config->header_count > 0)
    {
        client->hdr_names =
            (char **)malloc(config->header_count * sizeof(char *));
        client->hdr_values =
            (char **)malloc(config->header_count * sizeof(char *));
        if (client->hdr_names == NULL || client->hdr_values == NULL)
        {
            llm_client_free(client);
            set_error_code(error, LLM_ENOMEM);
            return NULL;
        }
        memset(client->hdr_names, 0, config->header_count * sizeof(char *));
        memset(client->hdr_values, 0, config->header_count * sizeof(char *));
        client->header_count = config->header_count;
        for (i = 0; i < config->header_count; i = i + 1)
        {
            client->hdr_names[i] = dup_str(config->headers[i].name);
            client->hdr_values[i] = dup_str(config->headers[i].value);
            if (client->hdr_names[i] == NULL || client->hdr_values[i] == NULL)
            {
                llm_client_free(client);
                set_error_code(error, LLM_ENOMEM);
                return NULL;
            }
        }
    }
    if (config->connect_timeout_ms <= 0)
    {
        client->connect_timeout_ms = LLM89_DEFAULT_CONNECT_TIMEOUT_MS;
    }
    else
    {
        client->connect_timeout_ms = config->connect_timeout_ms;
    }
    client->timeout_ms = config->timeout_ms;
    client->curl = curl_easy_init();
    if (client->curl == NULL)
    {
        llm_client_free(client);
        set_error_code(error, LLM_ENOMEM);
        return NULL;
    }
    return client;
}

void llm_client_free(llm_client *client)
{
    size_t i;
    if (client == NULL)
    {
        return;
    }
    if (client->curl != NULL)
    {
        curl_easy_cleanup(client->curl);
    }
    free(client->endpoint);
    free(client->api_key);
    if (client->hdr_names != NULL)
    {
        for (i = 0; i < client->header_count; i = i + 1)
        {
            free(client->hdr_names[i]);
        }
        free(client->hdr_names);
    }
    if (client->hdr_values != NULL)
    {
        for (i = 0; i < client->header_count; i = i + 1)
        {
            free(client->hdr_values[i]);
        }
        free(client->hdr_values);
    }
    free(client);
}

int llm_chat(llm_client *client, const llm_chat_request *request,
             llm_response *response, llm_cancel_fn cancel,
             void *cancel_userdata, llm_error *error)
{
    char *body;
    int rc;
    clear_error(error);
    if (client == NULL || request == NULL || response == NULL)
    {
        return err_ret(error, LLM_EINVAL);
    }
    memset(response, 0, sizeof(*response));
    rc = llm_json_build_request(request, 0, &body);
    if (rc != LLM_OK)
    {
        return err_ret(error, rc);
    }
    rc = run(client, body, strlen(body), response, NULL, NULL, cancel,
             cancel_userdata, error, 0, 1, 0);
    free(body);
    return rc;
}

int llm_chat_stream(llm_client *client, const llm_chat_request *request,
                    llm_stream_fn stream, void *stream_userdata,
                    llm_cancel_fn cancel, void *cancel_userdata,
                    llm_error *error)
{
    char *body;
    int rc;
    clear_error(error);
    if (client == NULL || request == NULL || stream == NULL)
    {
        return err_ret(error, LLM_EINVAL);
    }
    rc = llm_json_build_request(request, 1, &body);
    if (rc != LLM_OK)
    {
        return err_ret(error, rc);
    }
    rc = run(client, body, strlen(body), NULL, stream, stream_userdata, cancel,
             cancel_userdata, error, 1, 0, 1);
    free(body);
    return rc;
}

int llm_json(llm_client *client, const char *request_json,
             llm_response *response, llm_cancel_fn cancel,
             void *cancel_userdata, llm_error *error)
{
    clear_error(error);
    if (client == NULL || request_json == NULL || response == NULL)
    {
        return err_ret(error, LLM_EINVAL);
    }
    memset(response, 0, sizeof(*response));
    if (llm_json_validate_object(request_json, strlen(request_json)) != 0)
    {
        return err_ret(error, LLM_EJSON);
    }
    return run(client, request_json, strlen(request_json), response, NULL, NULL,
               cancel, cancel_userdata, error, 0, 0, 0);
}

int llm_json_stream(llm_client *client, const char *request_json,
                    llm_stream_fn stream, void *stream_userdata,
                    llm_cancel_fn cancel, void *cancel_userdata,
                    llm_error *error)
{
    clear_error(error);
    if (client == NULL || request_json == NULL || stream == NULL)
    {
        return err_ret(error, LLM_EINVAL);
    }
    if (llm_json_validate_object(request_json, strlen(request_json)) != 0)
    {
        return err_ret(error, LLM_EJSON);
    }
    return run(client, request_json, strlen(request_json), NULL, stream,
               stream_userdata, cancel, cancel_userdata, error, 1, 0, 0);
}

void llm_response_free(llm_response *response)
{
    if (response == NULL)
    {
        return;
    }
    free(response->text);
    free(response->json);
    memset(response, 0, sizeof(*response));
}
