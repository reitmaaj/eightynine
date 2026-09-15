/* tool/jrpc89.c - libjrpc89 CLI demo: send one JSON-RPC 2.0 request over an
 * already-open Unix socket fd and print the result or a structured error.
 *
 * Usage: jrpc89 <fd> <method> [params-json]
 *
 * The fd must be an open, connected Unix socket. The CLI takes ownership of
 * that inherited fd and closes it explicitly on every exit path; the library
 * never opens, connects, or closes it. SIGPIPE is ignored so a disconnected
 * peer surfaces as a write failure rather than process termination.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jrpc89.h>
#include <jrpc89_io.h>

#define JRPC89_BUF_LEN 8192

static int jrpc89_usage(const char *prog)
{
    fprintf(stderr, "usage: %s <fd> <method> [params-json]\n", prog);
    return 2;
}

static int jrpc89_usage_fd(const char *fd_text)
{
    fprintf(stderr, "jrpc89: invalid fd %s\n", fd_text);
    return 2;
}

/* Parse a non-negative fd with full validation. Returns 0 on success. */
static int jrpc89_parse_fd(const char *fd_text, int *fd)
{
    char *end;
    long v;
    errno = 0;
    end = NULL;
    v = strtol(fd_text, &end, 10);
    if (errno != 0)
    {
        return -1;
    }
    if (end == fd_text)
    {
        return -1;
    }
    if (*end != '\0')
    {
        return -1;
    }
    if (v < 0)
    {
        return -1;
    }
    if (v > INT_MAX)
    {
        return -1;
    }
    *fd = (int)v;
    return 0;
}

/* The optional params argument (argv[3]), or dflt when absent. */
static const char *jrpc89_arg3(int argc, char **argv, const char *dflt)
{
    if (argc >= 4)
    {
        return argv[3];
    }
    return dflt;
}

static void jrpc89_cleanup(int fd, j89_arena *a, j89_arena *out)
{
    close(fd);
    j89_arena_destroy(a);
    j89_arena_destroy(out);
}

/* Print "<msg>: <arena error>" and clean up. Returns 1. */
static int jrpc89_die(j89_arena *a, int fd, j89_arena *out, const char *msg)
{
    const char *e;
    e = j89_error(a);
    fprintf(stderr, "%s: %s\n", msg, e);
    jrpc89_cleanup(fd, a, out);
    return 1;
}

/* Print a fixed message and clean up. Returns 1. */
static int jrpc89_die_plain(int fd, j89_arena *a, j89_arena *out,
                            const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    jrpc89_cleanup(fd, a, out);
    return 1;
}

/* Parse a non-NULL params text into *params. Returns 1 when it parsed to a
 * real node, 0 on a parse error (message recorded in the arena). */
static int jrpc89_parse_params_text(j89_arena *a, const char *params_text,
                                    j89_len *params)
{
    j89_len plen;
    int ok;
    plen = strlen(params_text);
    *params = j89_parse(params_text, plen, a);
    ok = (*params != J89_BAD);
    if (ok == 0)
    {
        return 0;
    }
    return 1;
}

/* Parse params_text (when non-NULL) into *params. Returns 1 when the params
 * are usable (absent, or parsed to a real node), 0 on a parse error. */
static int jrpc89_parse_params(j89_arena *a, const char *params_text,
                               j89_len *params)
{
    *params = J89_BAD;
    if (params_text != NULL)
    {
        int st;
        st = jrpc89_parse_params_text(a, params_text, params);
        return st;
    }
    return 1;
}

static int jrpc89_print_result(j89_arena *a, j89_len result)
{
    j89_arena out;
    const void *rmem;
    j89_len roff;
    int r;
    j89_arena_init(&out);
    r = j89_render(a, result, 1, &out);
    if (r != 0)
    {
        j89_arena_destroy(&out);
        return -1;
    }
    rmem = out.mem;
    roff = out.off;
    fwrite(rmem, 1, roff, stdout);
    fputc('\n', stdout);
    j89_arena_destroy(&out);
    return 0;
}

static int jrpc89_print_error(const jrpc89_response *dec)
{
    j89_int code;
    int reserved;
    const char *cls;
    const char *msg;
    j89_len msglen;
    code = dec->error.code;
    reserved = jrpc89_error_code_reserved(code);
    cls = "application";
    if (reserved)
    {
        cls = "reserved";
    }
    msg = dec->error.message;
    msglen = dec->error.message_len;
    printf("error %.0f %s \"", code, cls);
    fwrite(msg, 1, msglen, stdout);
    printf("\"\n");
    return 0;
}

int main(int argc, char **argv)
{
    const char *fd_text;
    const char *method;
    const char *params_text;
    jrpc89_id id;
    jrpc89_response dec;
    jrpc89_status st;
    j89_arena a;
    j89_arena out;
    j89_len params;
    j89_len req;
    j89_len resp;
    j89_len len;
    j89_len mlen;
    char buf[JRPC89_BUF_LEN];
    int fd;
    int r;
    int match;
    int params_ok;
    int fd_ok;
    void (*sig_prev)(int);
    const char *wmem;
    j89_len woff;
    if (argc < 3)
    {
        int rc;
        rc = jrpc89_usage(argv[0]);
        return rc;
    }
    fd_text = argv[1];
    method = argv[2];
    params_text = jrpc89_arg3(argc, argv, NULL);
    fd_ok = jrpc89_parse_fd(fd_text, &fd);
    if (fd_ok != 0)
    {
        int rc;
        rc = jrpc89_usage_fd(fd_text);
        return rc;
    }
    sig_prev = signal(SIGPIPE, SIG_IGN);
    if (sig_prev == SIG_ERR)
    {
        fprintf(stderr, "jrpc89: cannot ignore SIGPIPE\n");
        return 1;
    }
    j89_arena_init(&a);
    j89_arena_init(&out);
    params_ok = jrpc89_parse_params(&a, params_text, &params);
    if (params_ok == 0)
    {
        int rc;
        rc = jrpc89_die(&a, fd, &out, "jrpc89: invalid params");
        return rc;
    }
    id.kind = JRPC89_ID_INT;
    id.num = 1;
    mlen = strlen(method);
    st = jrpc89_request_new(&a, method, mlen, params, &id, &req);
    if (st != JRPC89_OK)
    {
        int rc;
        rc = jrpc89_die(&a, fd, &out, "jrpc89: request failed");
        return rc;
    }
    r = j89_render(&a, req, 1, &out);
    if (r != 0)
    {
        int rc;
        rc = jrpc89_die(&a, fd, &out, "jrpc89: render failed");
        return rc;
    }
    wmem = out.mem;
    woff = out.off;
    st = jrpc89_fd_write_frame(fd, wmem, woff);
    if (st != JRPC89_OK)
    {
        int rc;
        rc = jrpc89_die_plain(fd, &a, &out, "jrpc89: write failed");
        return rc;
    }
    st = jrpc89_fd_read_frame(fd, buf, JRPC89_BUF_LEN, &len);
    if (st != JRPC89_OK)
    {
        int rc;
        rc = jrpc89_die_plain(fd, &a, &out, "jrpc89: read failed");
        return rc;
    }
    resp = j89_parse(buf, len, &a);
    if (resp == J89_BAD)
    {
        int rc;
        rc = jrpc89_die(&a, fd, &out, "jrpc89: invalid JSON");
        return rc;
    }
    st = jrpc89_response_decode(&a, resp, &dec);
    if (st != JRPC89_OK)
    {
        int rc;
        rc = jrpc89_die_plain(fd, &a, &out, "jrpc89: invalid response");
        return rc;
    }
    match = jrpc89_id_equal(&id, &dec.id);
    if (match == 0)
    {
        int rc;
        rc = jrpc89_die_plain(fd, &a, &out, "jrpc89: response id mismatch");
        return rc;
    }
    if (dec.kind == JRPC89_RESPONSE_ERROR)
    {
        r = jrpc89_print_error(&dec);
    }
    else
    {
        r = jrpc89_print_result(&a, dec.result);
    }
    close(fd);
    j89_arena_destroy(&a);
    j89_arena_destroy(&out);
    if (r != 0)
    {
        return 1;
    }
    return 0;
}
