/* conformance_norm.c - run the Unicode 17.0.0 NormalizationTest.txt suite
   against libu89 u89_normalize and report every mismatch. Usage:
     conformance_norm <path/to/NormalizationTest.txt>
   Exit 0 when all rows pass, nonzero otherwise. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "u89.h"

#define MAXLEN 4096

static unsigned char cbuf[5][MAXLEN];
static size_t clen[5];
static u89_cp work[16384];

/* Parse one ';'-delimited field of hex scalars into cbuf[col]. Returns 0 on
   success. */
static int parse_col(const char *field, int col)
{
    const char *p;
    unsigned char *o;
    u89_cp cp;
    char hex[9];
    int i;
    int n;

    clen[col] = 0;
    o = cbuf[col];
    p = field;
    while (*p != 0)
    {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        {
            p = p + 1;
        }
        if (*p == 0)
        {
            break;
        }
        i = 0;
        while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F')
               || (*p >= 'a' && *p <= 'f'))
        {
            if (i < 8)
            {
                hex[i] = *p;
                i = i + 1;
            }
            p = p + 1;
        }
        hex[i] = 0;
        cp = (u89_cp)strtoul(hex, NULL, 16);
        if (cp >= 0x80UL)
        {
            if (clen[col] + 4 > MAXLEN)
            {
                return 1;
            }
            n = u89_utf8_encode(cp, o + clen[col]);
            clen[col] = clen[col] + (size_t)n;
        }
        else
        {
            if (clen[col] + 1 > MAXLEN)
            {
                return 1;
            }
            o[clen[col]] = (unsigned char)cp;
            clen[col] = clen[col] + 1;
        }
    }
    return 0;
}

static int expect_col(int mode, const unsigned char *s, size_t n, int col)
{
    unsigned char dst[8192];
    int r;
    size_t want;

    r = u89_normalize_ex(mode, s, n, dst, sizeof dst, work, 16384);
    if (r < 0)
    {
        return 0;
    }
    want = clen[col];
    if ((size_t)r != want)
    {
        return 0;
    }
    if (memcmp(dst, cbuf[col], want) != 0)
    {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *fh;
    char line[65536];
    char *cols[5];
    char *save;
    char *tok;
    char *p;
    long line_no;
    long total;
    long failed;
    int ci;
    int pass;
    int any;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s NormalizationTest.txt\n", argv[0]);
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
    line_no = 0;
    while (fgets(line, sizeof line, fh) != NULL)
    {
        line_no = line_no + 1;
        if (line[0] == '#')
        {
            continue;
        }
        if (strncmp(line, "@Part", 5) == 0)
        {
            continue;
        }
        if (strncmp(line, "@Test", 5) == 0)
        {
            continue;
        }
        p = strchr(line, '#');
        if (p != NULL)
        {
            *p = 0;
        }
        any = 0;
        for (p = line; *p != 0; p = p + 1)
        {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            {
                any = 1;
                break;
            }
        }
        if (!any)
        {
            continue;
        }
        ci = 0;
        cols[0] = strtok_r(line, ";", &save);
        while (cols[ci] != NULL && ci < 5)
        {
            ci = ci + 1;
            cols[ci] = strtok_r(NULL, ";", &save);
        }
        if (ci < 5)
        {
            fprintf(stderr, "line %ld: expected 5 columns\n", line_no);
            failed = failed + 1;
            continue;
        }
        total = total + 1;
        pass = 1;
        for (ci = 0; ci < 5; ci = ci + 1)
        {
            if (parse_col(cols[ci], ci) != 0)
            {
                pass = 0;
            }
        }
        if (!pass)
        {
            fprintf(stderr, "line %ld: field too long\n", line_no);
            failed = failed + 1;
            continue;
        }
        /* canonical identities over the five columns. */
        if (!expect_col(0, cbuf[0], clen[0], 1))
        {
            pass = 0;
        }
        if (!expect_col(0, cbuf[2], clen[2], 1))
        {
            pass = 0;
        }
        if (!expect_col(0, cbuf[1], clen[1], 1))
        {
            pass = 0;
        }
        if (!expect_col(0, cbuf[3], clen[3], 3))
        {
            pass = 0;
        }
        if (!expect_col(0, cbuf[4], clen[4], 3))
        {
            pass = 0;
        }
        if (!expect_col(1, cbuf[0], clen[0], 2))
        {
            pass = 0;
        }
        if (!expect_col(1, cbuf[1], clen[1], 2))
        {
            pass = 0;
        }
        if (!expect_col(1, cbuf[2], clen[2], 2))
        {
            pass = 0;
        }
        if (!expect_col(1, cbuf[3], clen[3], 4))
        {
            pass = 0;
        }
        if (!expect_col(1, cbuf[4], clen[4], 4))
        {
            pass = 0;
        }
        if (!expect_col(2, cbuf[0], clen[0], 3))
        {
            pass = 0;
        }
        if (!expect_col(2, cbuf[1], clen[1], 3))
        {
            pass = 0;
        }
        if (!expect_col(2, cbuf[2], clen[2], 3))
        {
            pass = 0;
        }
        if (!expect_col(2, cbuf[3], clen[3], 3))
        {
            pass = 0;
        }
        if (!expect_col(2, cbuf[4], clen[4], 3))
        {
            pass = 0;
        }
        if (!expect_col(3, cbuf[0], clen[0], 4))
        {
            pass = 0;
        }
        if (!expect_col(3, cbuf[1], clen[1], 4))
        {
            pass = 0;
        }
        if (!expect_col(3, cbuf[2], clen[2], 4))
        {
            pass = 0;
        }
        if (!expect_col(3, cbuf[3], clen[3], 4))
        {
            pass = 0;
        }
        if (!expect_col(3, cbuf[4], clen[4], 4))
        {
            pass = 0;
        }
        if (!pass)
        {
            failed = failed + 1;
            if (failed <= 20)
            {
                fprintf(stderr, "FAIL line %ld\n", line_no);
            }
        }
    }
    fclose(fh);
    printf("NormalizationTest: %ld rows, %ld failed\n", total, failed);
    if (failed != 0)
    {
        return 1;
    }
    return 0;
}
