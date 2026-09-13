/* echo.c - the smallest complete librepl89 client: a blocking REPL that
 * echoes each submission, keeps history, and exits on EOF. */

#include <stdio.h>
#include <unistd.h>

#include "repl89.h"

#define ECHO_CONTINUE 0
#define ECHO_STOP 1
#define ECHO_FAIL 2

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
    fprintf(stderr, "echo: read error %d\n", err);
    return ECHO_FAIL;
}

static int echo_once(repl89 *r)
{
    repl89_text text;
    repl89_error err;
    repl89_result res;
    int rc;

    err = REPL89_OK;
    res = repl89_read(r, "> ", &text, &err);
    if (res == REPL89_RESULT_SUBMIT)
    {
        rc = echo_submit(r, &text);
        return rc;
    }
    if (res == REPL89_RESULT_CANCEL)
    {
        rc = echo_cancel();
        return rc;
    }
    if (res == REPL89_RESULT_EOF)
    {
        return ECHO_STOP;
    }
    rc = echo_error(err);
    return rc;
}

static int echo_run(repl89 *r)
{
    int rc;

    rc = ECHO_CONTINUE;
    while (rc == ECHO_CONTINUE)
    {
        rc = echo_once(r);
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
        fprintf(stderr, "echo: out of memory\n");
        return 1;
    }
    rc = echo_run(r);
    repl89_free(r);
    return rc;
}
