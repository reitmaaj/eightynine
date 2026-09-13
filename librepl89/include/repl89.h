#ifndef REPL89_H
#define REPL89_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct repl89 repl89;

typedef enum repl89_error {
    REPL89_OK = 0,
    REPL89_ENOMEM,
    REPL89_EIO,
    REPL89_ETTY,
    REPL89_EUTF8,
    REPL89_EINVAL,
    REPL89_ESTATE
} repl89_error;

typedef enum repl89_event {
    REPL89_EVENT_NONE = 0,
    REPL89_EVENT_SUBMIT,
    REPL89_EVENT_CANCEL,
    REPL89_EVENT_EOF
} repl89_event;

typedef enum repl89_result {
    REPL89_RESULT_SUBMIT = 0,
    REPL89_RESULT_CANCEL,
    REPL89_RESULT_EOF,
    REPL89_RESULT_ERROR
} repl89_result;

typedef struct repl89_text {
    const char *data;
    size_t len;
} repl89_text;

typedef struct repl89_config {
    int input_fd;
    int output_fd;
    unsigned int tab_width;
    size_t history_limit;
    const char *continuation_prompt;
} repl89_config;

repl89 *repl89_new(const repl89_config *config);
void repl89_free(repl89 *r);

repl89_error repl89_start(repl89 *r, const char *prompt);
repl89_error repl89_feed(repl89 *r, repl89_event *event);
repl89_result repl89_read(repl89 *r, const char *prompt, repl89_text *text,
                          repl89_error *error);

repl89_error repl89_submit(repl89 *r);
repl89_error repl89_cancel(repl89 *r);

repl89_error repl89_insert(repl89 *r, const char *text, size_t len);

repl89_text repl89_text_get(const repl89 *r);

repl89_error repl89_history_add(repl89 *r, const char *text, size_t len);
void repl89_history_clear(repl89 *r);

repl89_error repl89_resize(repl89 *r);
repl89_error repl89_hide(repl89 *r);
repl89_error repl89_show(repl89 *r);

#ifdef __cplusplus
}
#endif

#endif
