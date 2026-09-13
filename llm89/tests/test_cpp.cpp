// test_cpp.cpp - verifies llm89.h is C++-linkable through extern "C".
#include <cstddef>

#include "llm89.h"

namespace {

int (*p_global_init)(llm_error *) = &llm_global_init;
void (*p_global_cleanup)(void) = &llm_global_cleanup;
llm_client *(*p_client_new)(const llm_client_config *, llm_error *) =
    &llm_client_new;
void (*p_client_free)(llm_client *) = &llm_client_free;
int (*p_chat)(llm_client *, const llm_chat_request *, llm_response *,
              llm_cancel_fn, void *, llm_error *) = &llm_chat;
int (*p_chat_stream)(llm_client *, const llm_chat_request *, llm_stream_fn,
                     void *, llm_cancel_fn, void *, llm_error *) =
    &llm_chat_stream;
int (*p_json)(llm_client *, const char *, llm_response *, llm_cancel_fn,
              void *, llm_error *) = &llm_json;
int (*p_json_stream)(llm_client *, const char *, llm_stream_fn, void *,
                     llm_cancel_fn, void *, llm_error *) = &llm_json_stream;
void (*p_response_free)(llm_response *) = &llm_response_free;

}  // namespace

int main(void)
{
    llm_error error;
    llm_client_config cfg;
    cfg.endpoint = "https://example.invalid/v1/chat/completions";
    cfg.api_key = 0;
    cfg.headers = 0;
    cfg.header_count = 0;
    cfg.connect_timeout_ms = 1000;
    cfg.timeout_ms = 0;
    llm_global_init(&error);
    llm_client *client = llm_client_new(&cfg, &error);
    llm_client_free(client);
    llm_global_cleanup();
    (void)p_global_init;
    (void)p_global_cleanup;
    (void)p_client_new;
    (void)p_client_free;
    (void)p_chat;
    (void)p_chat_stream;
    (void)p_json;
    (void)p_json_stream;
    (void)p_response_free;
    return 0;
}
