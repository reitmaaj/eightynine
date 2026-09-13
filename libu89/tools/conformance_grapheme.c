/* conformance_grapheme.c - run the Unicode 17.0.0 GraphemeBreakTest.txt suite
   against libu89 extended grapheme cluster boundaries. Usage:
     conformance_grapheme <path/to/GraphemeBreakTest.txt>
   Every row is checked four ways: boundary predicate, forward traversal,
   backward traversal, and the prev(next(p)) == p inverse relation.
   Exit 0 when all rows pass, nonzero otherwise. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "u89.h"

#define MAX_TOK 1024
#define MAX_CP 512
#define MAX_BYTES 4096

static u89_cp cps[MAX_CP];
static unsigned char data[MAX_BYTES];
static size_t starts[MAX_CP];
static int brk[MAX_CP];
static char *toks[MAX_TOK];
static size_t bounds[MAX_CP + 2];

static int is_div(const char *t)
{
    return strcmp(t, "\xC3\xB7") == 0;
}

static int is_mul(const char *t)
{
    return strcmp(t, "\xC3\x97") == 0;
}

/* Returns 1 on pass, 0 on failure, -1 when the row is not parseable. */
static int check_line(const char *line)
{
    char buf[8192];
    char *save;
    char *tok;
    size_t ntok;
    size_t ncps;
    size_t nbytes;
    size_t nb;
    size_t i;
    size_t j;
    size_t p;
    size_t q;
    size_t k;
    int end_break;
    int pass;
    u89_cp cp;
    int len;

    strncpy(buf, line, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    tok = strchr(buf, '#');
    if (tok != NULL)
    {
        *tok = 0;
    }
    ntok = 0;
    tok = strtok_r(buf, " \t\r\n", &save);
    while (tok != NULL && ntok < MAX_TOK)
    {
        toks[ntok] = tok;
        ntok = ntok + 1;
        tok = strtok_r(NULL, " \t\r\n", &save);
    }
    if (ntok < 3)
    {
        return -1;
    }
    ncps = 0;
    nbytes = 0;
    end_break = 0;
    for (i = 0; i + 1 < ntok; i = i + 2)
    {
        const char *m;
        const char *h;

        m = toks[i];
        h = toks[i + 1];
        if (!is_div(m) && !is_mul(m))
        {
            return -1;
        }
        if (ncps >= MAX_CP)
        {
            return -1;
        }
        cp = (u89_cp)strtoul(h, NULL, 16);
        starts[ncps] = nbytes;
        brk[ncps] = is_div(m);
        len = u89_utf8_encode(cp, data + nbytes);
        if (len == 0)
        {
            return -1;
        }
        if (nbytes + (size_t)len > MAX_BYTES)
        {
            return -1;
        }
        nbytes = nbytes + (size_t)len;
        ncps = ncps + 1;
    }
    if (brk[0] != 1)
    {
        return -1;
    }
    end_break = is_div(toks[ntok - 1]);

    nb = 0;
    bounds[nb] = 0;
    nb = nb + 1;
    for (i = 1; i < ncps; i++)
    {
        if (brk[i])
        {
            bounds[nb] = starts[i];
            nb = nb + 1;
        }
    }
    if (end_break)
    {
        bounds[nb] = nbytes;
        nb = nb + 1;
    }

    pass = 1;
    for (i = 0; i < nb; i++)
    {
        if (!u89_grapheme_boundary(data, nbytes, bounds[i]))
        {
            pass = 0;
        }
    }
    for (i = 1; i < ncps; i++)
    {
        if (brk[i])
        {
            continue;
        }
        if (u89_grapheme_boundary(data, nbytes, starts[i]))
        {
            pass = 0;
        }
    }
    for (i = 0; i < nbytes; i++)
    {
        int at_start;

        at_start = 0;
        for (j = 0; j < ncps; j++)
        {
            if (starts[j] == i)
            {
                at_start = 1;
            }
        }
        if (!at_start)
        {
            if (u89_grapheme_boundary(data, nbytes, i))
            {
                pass = 0;
            }
        }
    }

    p = 0;
    k = 0;
    while (p < nbytes)
    {
        q = u89_grapheme_next(data, nbytes, p);
        if (q <= p || q > nbytes)
        {
            pass = 0;
            break;
        }
        if (k + 1 >= nb || bounds[k + 1] != q)
        {
            pass = 0;
            break;
        }
        p = q;
        k = k + 1;
    }
    if (p != nbytes || k != nb - 1)
    {
        pass = 0;
    }

    p = nbytes;
    k = nb - 1;
    while (p > 0)
    {
        q = u89_grapheme_prev(data, nbytes, p);
        if (q >= p)
        {
            pass = 0;
            break;
        }
        if (k == 0 || bounds[k - 1] != q)
        {
            pass = 0;
            break;
        }
        p = q;
        k = k - 1;
    }
    if (p != 0 || k != 0)
    {
        pass = 0;
    }

    for (i = 1; i < nb; i++)
    {
        if (u89_grapheme_prev(data, nbytes, bounds[i]) != bounds[i - 1])
        {
            pass = 0;
        }
    }
    return pass;
}

int main(int argc, char **argv)
{
    FILE *fh;
    char line[65536];
    long line_no;
    long total;
    long failed;
    long skipped;
    int r;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s GraphemeBreakTest.txt\n", argv[0]);
        return 2;
    }
    fh = fopen(argv[1], "r");
    if (fh == NULL)
    {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    total = 0;
    failed = 0;
    skipped = 0;
    line_no = 0;
    while (fgets(line, sizeof line, fh) != NULL)
    {
        line_no = line_no + 1;
        if (line[0] == '#')
        {
            continue;
        }
        if (strncmp(line, "@", 1) == 0)
        {
            continue;
        }
        r = check_line(line);
        if (r < 0)
        {
            skipped = skipped + 1;
            continue;
        }
        total = total + 1;
        if (r == 0)
        {
            failed = failed + 1;
            if (failed <= 20)
            {
                fprintf(stderr, "FAIL line %ld\n", line_no);
            }
        }
    }
    fclose(fh);
    printf("GraphemeBreakTest: %ld rows, %ld failed, %ld skipped\n", total,
           failed, skipped);
    if (failed != 0)
    {
        return 1;
    }
    return 0;
}
