/* echo_events.c - event-driven librepl89 client: a REPL that echoes each
 * submission, keeps history, and exits on EOF. Unlike the blocking example
 * it owns a SIGWINCH handler that only sets a flag; the event loop observes
 * the flag and calls repl89_resize, so resizing redraws immediately even
 * while feed is blocked in read. */

#define _DEFAULT_SOURCE 1

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "repl89.h"

#define ECHO_CONTINUE 0
#define ECHO_STOP 1
#define ECHO_FAIL 2

static volatile sig_atomic_t g_resized = 0;

static void winch_handler(int sig)
{
    (void)sig;
    g_resized = 1;
}

/* SA_RESTART must stay clear so a blocked read reports EINTR and the loop
   can observe the flag without waiting for the next keypress. */
static void install_winch(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = winch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);
}

static int echo_write(const repl89_text *text)
{
    size_t n;
    int c;

    n = fwrite(text->data, 1, text->len, stdout);
    if (n != text->len)
    {
        return ECHO_FAIL;
    }
    c = fputc('\n', stdout);
    if (c == EOF)
    {
        return ECHO_FAIL;
    }
    fflush(stdout);
    return ECHO_CONTINUE;
}

static int echo_submit(repl89 *r, const repl89_text *text)
{
    repl89_error err;
    int rc;

    rc = echo_write(text);
    if (rc != ECHO_CONTINUE)
    {
        return ECHO_FAIL;
    }
    err = repl89_history_add(r, text->data, text->len);
    if (err != REPL89_OK)
    {
        return ECHO_FAIL;
    }
    return ECHO_CONTINUE;
}

static int echo_cancel(void)
{
    int rc;

    rc = fputs("^C\n", stdout);
    if (rc == EOF)
    {
        return ECHO_FAIL;
    }
    fflush(stdout);
    return ECHO_CONTINUE;
}

static int echo_error(repl89_error err)
{
    fprintf(stderr, "echo-events: error %d\n", err);
    return ECHO_FAIL;
}

static int echo_fail(repl89_error err)
{
    int rc;

    rc = echo_error(err);
    return rc;
}

static int echo_resize(repl89 *r)
{
    repl89_error err;
    int rc;

    if (g_resized == 0)
    {
        return ECHO_CONTINUE;
    }
    g_resized = 0;
    err = repl89_resize(r);
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    return ECHO_CONTINUE;
}

static int echo_after_cancel(repl89 *r)
{
    repl89_error err;
    int rc;

    err = repl89_cancel(r);
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    rc = echo_cancel();
    if (rc != ECHO_CONTINUE)
    {
        return rc;
    }
    err = repl89_start(r, "> ");
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    return ECHO_CONTINUE;
}

static int echo_event(repl89 *r, repl89_event ev)
{
    repl89_text text;
    repl89_error err;
    int rc;

    if (ev == REPL89_EVENT_CANCEL)
    {
        rc = echo_after_cancel(r);
        return rc;
    }
    if (ev == REPL89_EVENT_EOF)
    {
        return ECHO_STOP;
    }
    if (ev != REPL89_EVENT_SUBMIT)
    {
        return ECHO_CONTINUE;
    }
    text = repl89_text_get(r);
    err = repl89_submit(r);
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    rc = echo_submit(r, &text);
    if (rc != ECHO_CONTINUE)
    {
        return rc;
    }
    err = repl89_start(r, "> ");
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    return ECHO_CONTINUE;
}

static int echo_feed_fail(repl89 *r, repl89_error err)
{
    int rc;

    repl89_cancel(r);
    rc = echo_fail(err);
    return rc;
}

static int echo_loop(repl89 *r)
{
    repl89_event ev;
    repl89_error err;
    int rc;

    err = repl89_start(r, "> ");
    if (err != REPL89_OK)
    {
        rc = echo_fail(err);
        return rc;
    }
    rc = echo_resize(r);
    while (rc == ECHO_CONTINUE)
    {
        err = repl89_feed(r, &ev);
        if (err != REPL89_OK)
        {
            rc = echo_feed_fail(r, err);
            return rc;
        }
        rc = echo_event(r, ev);
        if (rc == ECHO_CONTINUE)
        {
            rc = echo_resize(r);
        }
    }
    if (rc == ECHO_STOP)
    {
        return 0;
    }
    return 1;
}

int main(void)
{
    repl89_config cfg;
    repl89 *r;
    int rc;

    cfg.input_fd = STDIN_FILENO;
    cfg.output_fd = STDOUT_FILENO;
    cfg.tab_width = 8;
    cfg.history_limit = 100;
    cfg.continuation_prompt = "... ";
    r = repl89_new(&cfg);
    if (r == NULL)
    {
        fprintf(stderr, "echo-events: out of memory\n");
        return 1;
    }
    install_winch();
    rc = echo_loop(r);
    repl89_free(r);
    return rc;
}
