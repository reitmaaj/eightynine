#ifndef LLM89_H
#define LLM89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LLM89_VERSION_MAJOR 0
#define LLM89_VERSION_MINOR 1
#define LLM89_VERSION_PATCH 0

#define LLM_OK 0
#define LLM_EINVAL 1
#define LLM_ENOMEM 2
#define LLM_ECURL 3
#define LLM_EHTTP 4
#define LLM_EJSON 5
#define LLM_ESSE 6
#define LLM_EPROTO 7
#define LLM_ECANCELLED 8
#define LLM_EOVERFLOW 9

#define LLM_ROLE_SYSTEM 1
#define LLM_ROLE_USER 2
#define LLM_ROLE_ASSISTANT 3

#define LLM_EVENT_DATA 1
#define LLM_EVENT_DONE 2

    typedef struct llm_client llm_client;

    typedef struct llm_error
    {
        int code;
        long http_status;
        long transport_code;
        char message[256];
    } llm_error;

    typedef struct llm_header
    {
        const char *name;
        const char *value;
    } llm_header;

    typedef struct llm_client_config
    {
        const char *endpoint;
        const char *api_key;
        const llm_header *headers;
        size_t header_count;
        long connect_timeout_ms;
        long timeout_ms;
    } llm_client_config;

    typedef struct llm_message
    {
        int role;
        const char *content;
    } llm_message;

    typedef struct llm_chat_request
    {
        const char *model;
        const llm_message *messages;
        size_t message_count;

        int set_temperature;
        double temperature;

        int set_max_tokens;
        long max_tokens;
    } llm_chat_request;

    typedef struct llm_response
    {
        char *text;
        char *json;
        long http_status;
    } llm_response;

    typedef struct llm_stream_event
    {
        int type;

        const char *text;
        size_t text_len;

        const char *json;
        size_t json_len;
    } llm_stream_event;

    typedef int (*llm_stream_fn)(void *userdata, const llm_stream_event *event);

    typedef int (*llm_cancel_fn)(void *userdata);

    int llm_global_init(llm_error *error);
    void llm_global_cleanup(void);

    llm_client *llm_client_new(const llm_client_config *config,
                               llm_error *error);

    void llm_client_free(llm_client *client);

    int llm_chat(llm_client *client, const llm_chat_request *request,
                 llm_response *response, llm_cancel_fn cancel,
                 void *cancel_userdata, llm_error *error);

    int llm_chat_stream(llm_client *client, const llm_chat_request *request,
                        llm_stream_fn stream, void *stream_userdata,
                        llm_cancel_fn cancel, void *cancel_userdata,
                        llm_error *error);

    int llm_json(llm_client *client, const char *request_json,
                 llm_response *response, llm_cancel_fn cancel,
                 void *cancel_userdata, llm_error *error);

    int llm_json_stream(llm_client *client, const char *request_json,
                        llm_stream_fn stream, void *stream_userdata,
                        llm_cancel_fn cancel, void *cancel_userdata,
                        llm_error *error);

    void llm_response_free(llm_response *response);

#ifdef __cplusplus
}
#endif

#endif
