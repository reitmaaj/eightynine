/* repl89_key.c - pure byte-stream key decoder.
 *
 * Decodes the narrow ECMA-48/xterm key set the editor supports, plus ordinary
 * UTF-8 text, from an arbitrary byte range. Incomplete escape sequences
 * report zero consumed bytes so the caller can retain them across reads.
 * Unknown complete sequences are consumed and ignored, never partially
 * reinterpreted. Bracketed-paste markers are reported as their own keys. */

#include "repl89_internal.h"
#include "u89.h"

#define KEY_ESC 0x1B
#define KEY_SEQ_CAP 32

static void key_set(repl89_key *key, repl89_key_type type)
{
    key->type = type;
    key->len = 0;
}

static void key_text_len(repl89_key *key, size_t len)
{
    key_set(key, REPL89_KEY_TEXT);
    key->len = len;
}

static void key_text(repl89_key *key)
{
    key_text_len(key, 1);
}

static size_t key_control(unsigned char b, repl89_key *key)
{
    if (b == 0x01)
    {
        key_set(key, REPL89_KEY_HOME);
        return 1;
    }
    if (b == 0x03)
    {
        key_set(key, REPL89_KEY_CANCEL);
        return 1;
    }
    if (b == 0x04)
    {
        key_set(key, REPL89_KEY_CTRL_D);
        return 1;
    }
    if (b == 0x05)
    {
        key_set(key, REPL89_KEY_END);
        return 1;
    }
    if (b == 0x08)
    {
        key_set(key, REPL89_KEY_BACKSPACE);
        return 1;
    }
    if (b == 0x09)
    {
        key_text(key);
        return 1;
    }
    if (b == 0x0A)
    {
        key_set(key, REPL89_KEY_LF);
        return 1;
    }
    if (b == 0x0B)
    {
        key_set(key, REPL89_KEY_CTRL_K);
        return 1;
    }
    if (b == 0x0D)
    {
        key_set(key, REPL89_KEY_SUBMIT);
        return 1;
    }
    if (b == 0x0E)
    {
        key_set(key, REPL89_KEY_CTRL_N);
        return 1;
    }
    if (b == 0x10)
    {
        key_set(key, REPL89_KEY_CTRL_P);
        return 1;
    }
    if (b == 0x15)
    {
        key_set(key, REPL89_KEY_CTRL_U);
        return 1;
    }
    key_set(key, REPL89_KEY_IGNORE);
    return 1;
}

static size_t step_next(size_t i)
{
    return i + 1;
}

static unsigned char byte_at(const char *p, size_t i)
{
    return (unsigned char)p[i];
}

static void param_digit(int *cur, int *has, unsigned char c)
{
    *cur = *cur * 10 + (c - '0');
    *has = 1;
}

static int param_take(int *cur, int *has, unsigned char c)
{
    if (c >= '0')
    {
        if (c <= '9')
        {
            param_digit(cur, has, c);
            return 1;
        }
    }
    if (c == ';')
    {
        return 2;
    }
    return 0;
}

static void param_store(int *params, int i, int value)
{
    params[i] = value;
}

static void param_flush(int *params, int *np, int *cur, int *has)
{
    int value;

    if (*np >= 2)
    {
        return;
    }
    if (*has == 0)
    {
        return;
    }
    value = *cur;
    param_store(params, *np, value);
    *np = *np + 1;
    *cur = 0;
    *has = 0;
}

static void param_semi(int *params, int *np, int *cur, int *has, size_t *i)
{
    param_flush(params, np, cur, has);
    *i = *i + 1;
}

static int param_first(const int *params, int np)
{
    if (np > 0)
    {
        return params[0];
    }
    return 0;
}

typedef struct param_scan
{
    int params[2];
    int np;
    int cur;
    int has;
    size_t i;
} param_scan;

/* Consume one parameter byte. Returns 1 to continue, 0 at the final byte. */
static int param_step(const char *p, size_t len, param_scan *s)
{
    unsigned char c;
    int r;

    if (s->i >= len)
    {
        return 0;
    }
    c = byte_at(p, s->i);
    r = param_take(&s->cur, &s->has, c);
    if (r == 0)
    {
        return 0;
    }
    if (r == 1)
    {
        s->i = step_next(s->i);
        return 1;
    }
    param_semi(s->params, &s->np, &s->cur, &s->has, &s->i);
    return 1;
}

/* Parse up to two numeric parameters from [p, p+len). */
static int parse_params(const char *p, size_t len, int *params)
{
    param_scan s;
    int go;

    s.params[0] = 0;
    s.params[1] = 0;
    s.np = 0;
    s.cur = 0;
    s.has = 0;
    s.i = 0;
    if (s.i < len)
    {
        if (p[s.i] == '?')
        {
            s.i = step_next(s.i);
        }
    }
    go = 1;
    while (go != 0)
    {
        go = param_step(p, len, &s);
    }
    param_flush(s.params, &s.np, &s.cur, &s.has);
    params[0] = s.params[0];
    params[1] = s.params[1];
    return s.np;
}

/* Index of the CSI final byte within the first KEY_SEQ_CAP bytes, or n when
   no final byte is present in that window. The window keeps the decoder's
   behavior independent of how reads are fragmented. */
static size_t csi_final_at(const char *p, size_t n)
{
    size_t i;
    size_t limit;
    unsigned char c;

    limit = n;
    if (limit > KEY_SEQ_CAP)
    {
        limit = KEY_SEQ_CAP;
    }
    i = 2;
    if (i < limit)
    {
        if (p[i] == '?')
        {
            i = step_next(i);
        }
    }
    while (i < limit)
    {
        c = byte_at(p, i);
        if (c >= 0x40)
        {
            if (c <= 0x7E)
            {
                return i;
            }
        }
        i = step_next(i);
    }
    return n;
}

static void key_apply_tilde(const int *params, int np, repl89_key *key)
{
    int p0;

    p0 = param_first(params, np);
    if (p0 == 1)
    {
        key_set(key, REPL89_KEY_HOME);
    }
    if (p0 == 3)
    {
        key_set(key, REPL89_KEY_DELETE);
    }
    if (p0 == 4)
    {
        key_set(key, REPL89_KEY_END);
    }
    if (p0 == 200)
    {
        key_set(key, REPL89_KEY_PASTE_BEGIN);
    }
    if (p0 == 201)
    {
        key_set(key, REPL89_KEY_PASTE_END);
    }
}

static void key_apply_csi(unsigned char final, const int *params, int np,
                          repl89_key *key)
{
    if (final == 'A')
    {
        key_set(key, REPL89_KEY_UP);
    }
    if (final == 'B')
    {
        key_set(key, REPL89_KEY_DOWN);
    }
    if (final == 'C')
    {
        key_set(key, REPL89_KEY_RIGHT);
    }
    if (final == 'D')
    {
        key_set(key, REPL89_KEY_LEFT);
    }
    if (final == 'H')
    {
        key_set(key, REPL89_KEY_HOME);
    }
    if (final == 'F')
    {
        key_set(key, REPL89_KEY_END);
    }
    if (final == '~')
    {
        key_apply_tilde(params, np, key);
    }
}

static size_t key_csi(const char *p, size_t n, repl89_key *key)
{
    size_t fin;
    int params[2];
    int np;
    unsigned char final;

    fin = csi_final_at(p, n);
    if (fin >= n)
    {
        if (n >= KEY_SEQ_CAP)
        {
            key_set(key, REPL89_KEY_IGNORE);
            return KEY_SEQ_CAP;
        }
        return 0;
    }
    np = parse_params(p + 2, fin - 2, params);
    final = (unsigned char)p[fin];
    key_set(key, REPL89_KEY_IGNORE);
    key_apply_csi(final, params, np, key);
    return fin + 1;
}

static size_t key_ss3(const char *p, size_t n, repl89_key *key)
{
    unsigned char f;

    if (n < 3)
    {
        return 0;
    }
    f = (unsigned char)p[2];
    key_set(key, REPL89_KEY_IGNORE);
    if (f == 'A')
    {
        key_set(key, REPL89_KEY_UP);
    }
    if (f == 'B')
    {
        key_set(key, REPL89_KEY_DOWN);
    }
    if (f == 'C')
    {
        key_set(key, REPL89_KEY_RIGHT);
    }
    if (f == 'D')
    {
        key_set(key, REPL89_KEY_LEFT);
    }
    if (f == 'H')
    {
        key_set(key, REPL89_KEY_HOME);
    }
    if (f == 'F')
    {
        key_set(key, REPL89_KEY_END);
    }
    return 3;
}

static size_t key_escape(const char *p, size_t n, repl89_key *key)
{
    size_t used;

    if (n < 2)
    {
        return 0;
    }
    if (p[1] == '[')
    {
        used = key_csi(p, n, key);
        return used;
    }
    if (p[1] == 'O')
    {
        used = key_ss3(p, n, key);
        return used;
    }
    if (p[1] == '\r')
    {
        key_set(key, REPL89_KEY_LF);
        return 2;
    }
    key_set(key, REPL89_KEY_IGNORE);
    return 2;
}

size_t repl89_key_next(const char *p, size_t n, repl89_key *key)
{
    unsigned char b;
    u89_cp cp;
    u89_status st;
    size_t next;
    int need;

    if (n == 0)
    {
        return 0;
    }
    b = (unsigned char)p[0];
    if (b == KEY_ESC)
    {
        next = key_escape(p, n, key);
        return next;
    }
    if (b == 0x7F)
    {
        key_set(key, REPL89_KEY_BACKSPACE);
        return 1;
    }
    if (b < 0x20)
    {
        next = key_control(b, key);
        return next;
    }
    cp = 0;
    next = 0;
    st = u89_utf8_decode((const unsigned char *)p, n, 0, &cp, &next);
    if (st == U89_OK)
    {
        key_text_len(key, next);
        return next;
    }
    need = u89_utf8_seq_len(b);
    if (need > 0)
    {
        if (n < (size_t)need)
        {
            return 0;
        }
    }
    key_set(key, REPL89_KEY_IGNORE);
    return 1;
}
