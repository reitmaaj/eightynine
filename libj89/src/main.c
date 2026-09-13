/* main.c - libj89 CLI: read JSON, print canonical JSON or an error. */
#include <stdio.h>
#include <stdlib.h>

#include "../include/j89.h"

/* Double the allocation backing *pbuf. Updates *pbuf and *pcap on success.
 * Returns 0 on success, -1 on allocation failure (buffer left intact). */
static int grow_buf(char **pbuf, j89_len *pcap)
{
    char *nbuf;
    j89_len ncap;
    void *nv;
    ncap = *pcap * 2;
    nv = realloc(*pbuf, ncap);
    if (nv == NULL)
    {
        return -1;
    }
    nbuf = (char *)nv;
    *pbuf = nbuf;
    *pcap = ncap;
    return 0;
}

/* Read one chunk of the remaining capacity of `buf` into the free tail at
 * offset `len`. Returns the number of bytes read (0 means EOF). */
static size_t do_read(FILE *f, char *buf, j89_len cap, j89_len len)
{
    char *p;
    j89_len n;
    size_t got;
    p = buf + len;
    n = cap - len;
    got = fread(p, 1, n, f);
    return got;
}

/* Advance the stream by one read. Returns 0 to keep reading, 1 on EOF, or
 * -1 on allocation failure. Updates *pbuf, *pcap and *plen as it grows and
 * consumes input. */
static int read_more(FILE *f, char **pbuf, j89_len *pcap, j89_len *plen)
{
    size_t got;
    int grow;
    if (*plen == *pcap)
    {
        grow = grow_buf(pbuf, pcap);
        if (grow != 0)
        {
            return -1;
        }
    }
    got = do_read(f, *pbuf, *pcap, *plen);
    *plen = *plen + got;
    if (got == 0)
    {
        return 1;
    }
    return 0;
}

/* Read the whole of `f` into a freshly allocated buffer. Returns the buffer
 * (NUL-free, raw bytes) and sets *outlen, or NULL on allocation failure. */
static char *read_all(FILE *f, j89_len *outlen)
{
    j89_len cap;
    j89_len len;
    char *buf;
    void *vp0;
    int st;
    cap = 4096;
    len = 0;
    vp0 = malloc(cap);
    buf = (char *)vp0;
    if (buf == NULL)
    {
        return NULL;
    }
    st = 0;
    while (st == 0)
    {
        st = read_more(f, &buf, &cap, &len);
    }
    if (st < 0)
    {
        free(buf);
        return NULL;
    }
    *outlen = len;
    return buf;
}

/* Write the single byte at p[i] to stdout. */
static void write_byte(char *p, j89_len i)
{
    char c;
    int ci;
    c = p[i];
    ci = (int)c;
    fputc(ci, stdout);
}

/* Write every byte in the `out` arena's used region to stdout, then a
 * trailing newline. */
static void emit_bytes(j89_arena *out)
{
    void *m;
    char *p;
    j89_len n;
    j89_len i;
    m = out->mem;
    p = (char *)m;
    n = out->off;
    for (i = 0; i < n; i = i + 1)
    {
        write_byte(p, i);
    }
    fputc('\n', stdout);
}

/* Report that reading `name` ran out of memory and return the exit code. */
static int err_oom(const char *name, j89_arena *a, j89_arena *out)
{
    fprintf(stderr, "j89: out of memory reading %s\n", name);
    j89_arena_destroy(a);
    j89_arena_destroy(out);
    return 2;
}

/* True when `root` is the sentinel bad node index. */
static int is_bad(j89_len root)
{
    j89_len sent;
    int eq;
    sent = (j89_len)-1;
    eq = (root == sent);
    return eq;
}

/* Report a parse error for `name` and return the exit code. */
static int err_parse(const char *name, char *buf, j89_arena *a, j89_arena *out)
{
    const char *e;
    e = j89_error(a);
    fprintf(stderr, "j89: %s: %s\n", name, e);
    free(buf);
    j89_arena_destroy(a);
    j89_arena_destroy(out);
    return 1;
}

/* Report that rendering failed for `name` and return the exit code. */
static int err_render(const char *name, char *buf, j89_arena *a, j89_arena *out)
{
    fprintf(stderr, "j89: render failed for %s\n", name);
    free(buf);
    j89_arena_destroy(a);
    j89_arena_destroy(out);
    return 2;
}

/* Read, parse, and print one input stream, returning the process exit code. */
static int run_input(FILE *f, const char *name)
{
    j89_arena a;
    j89_arena out;
    char *buf;
    j89_len len;
    j89_len root;
    int rc;
    int parsebad;
    int failed;
    j89_arena_init(&a);
    j89_arena_init(&out);
    buf = read_all(f, &len);
    if (buf == NULL)
    {
        int st;
        st = err_oom(name, &a, &out);
        return st;
    }
    root = j89_parse(buf, len, &a);
    parsebad = is_bad(root);
    if (parsebad)
    {
        int st;
        st = err_parse(name, buf, &a, &out);
        return st;
    }
    rc = j89_render(&a, root, 1, &out);
    if (rc != 0)
    {
        int st;
        st = err_render(name, buf, &a, &out);
        return st;
    }
    failed = out.failed;
    if (failed)
    {
        int st;
        st = err_render(name, buf, &a, &out);
        return st;
    }
    emit_bytes(&out);
    free(buf);
    j89_arena_destroy(&a);
    j89_arena_destroy(&out);
    return 0;
}

/* Read, parse, and print the file at `path`, returning the exit code. */
static int run_file(const char *path)
{
    FILE *f;
    int rc;
    f = fopen(path, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "j89: cannot open %s\n", path);
        return 2;
    }
    rc = run_input(f, path);
    fclose(f);
    return rc;
}

/* Process the file named by argv[1], returning the exit code. */
static int run_file_arg(char **argv)
{
    const char *path;
    int rc;
    path = argv[1];
    rc = run_file(path);
    return rc;
}

int main(int argc, char **argv)
{
    int rc;
    if (argc > 1)
    {
        rc = run_file_arg(argv);
        return rc;
    }
    rc = run_input(stdin, "<stdin>");
    return rc;
}
