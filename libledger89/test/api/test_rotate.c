/* test_rotate.c - R01..R13: explicit and automatic rotation, sealed
 * immutability, and cross-segment access. */

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "test.h"

#include "fixture.h"

static int count_sealed(const char *path)
{
    DIR *d;
    struct dirent *e;
    int n;

    d = opendir(path);
    if (d == NULL)
    {
        return -1;
    }
    n = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strlen(e->d_name) == 24u)
        {
            if (strcmp(e->d_name + 20, ".seg") == 0)
            {
                ++n;
            }
        }
    }
    closedir(d);
    return n;
}

static int find_sealed(const char *path, char *name, size_t cap)
{
    DIR *d;
    struct dirent *e;
    int found;

    d = opendir(path);
    if (d == NULL)
    {
        return -1;
    }
    found = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strlen(e->d_name) == 24u)
        {
            if (strcmp(e->d_name + 20, ".seg") == 0)
            {
                if (strlen(e->d_name) + 1u <= cap)
                {
                    memcpy(name, e->d_name, strlen(e->d_name) + 1u);
                    found = 1;
                    break;
                }
            }
        }
    }
    closedir(d);
    return found;
}

static int read_file(const char *dir, const char *name, unsigned char *buf,
                     size_t cap, size_t *n)
{
    char path[256];
    FILE *fp;
    size_t dl;
    size_t nl;
    size_t got;

    dl = strlen(dir);
    nl = strlen(name);
    if (dl + nl + 2u > sizeof path)
    {
        return -1;
    }
    memcpy(path, dir, dl);
    path[dl] = '/';
    memcpy(path + dl + 1u, name, nl + 1u);
    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return -1;
    }
    got = fread(buf, 1u, cap, fp);
    fclose(fp);
    *n = got;
    return 0;
}

static int append_tag(fx *f, ledger89_index index, unsigned long tag)
{
    unsigned char data[1];

    data[0] = 'r';
    return fx_append(f, index, tag, data, sizeof data);
}

int main(void)
{
    fx f;
    fx g;
    ledger89_view v;
    unsigned char first[4096];
    unsigned char second[4096];
    char name[64];
    size_t n1;
    size_t n2;
    ledger89_index i;

    /* R01: rotating an empty active segment is a no-op. */
    CHECK_EQ(fx_open_limits(&f, 4096ul, 0ul), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    CHECK_EQ(count_sealed(f.path), 0);
    CHECK_EQ(ledger89_last_index(f.l), 0ul);

    /* R02/R03b: rotate after one record; sealed bytes are immutable. */
    CHECK_EQ(append_tag(&f, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    CHECK_EQ(count_sealed(f.path), 1);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK(find_sealed(f.path, name, sizeof name) == 1);
    CHECK_EQ(read_file(f.path, name, first, sizeof first, &n1), 0);
    CHECK(n1 > 0u);

    /* R10: rotating again with an empty active segment is a no-op. */
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    CHECK_EQ(count_sealed(f.path), 1);

    /* R11/R12: append after rotation and read across the boundary. */
    CHECK_EQ(append_tag(&f, 2ul, 2ul), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    CHECK_EQ(v.index, 2ul);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    CHECK_EQ(count_sealed(f.path), 2);
    CHECK_EQ(read_file(f.path, name, second, sizeof second, &n2), 0);
    CHECK_EQ(n1, n2);
    CHECK(memcmp(first, second, n1) == 0);

    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 2ul);
    CHECK_EQ(fx_read(&f, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(fx_read(&f, 2ul, &v), LEDGER89_OK);
    fx_close(&f);

    /* R04/R05: the record threshold seals before the exceeding batch. */
    CHECK_EQ(fx_open_limits(&g, 0ul, 3ul), LEDGER89_OK);
    for (i = 1ul; i <= 6ul; ++i)
    {
        CHECK_EQ(append_tag(&g, i, i), LEDGER89_OK);
    }
    CHECK_EQ(count_sealed(g.path), 1);
    CHECK_EQ(append_tag(&g, 7ul, 7ul), LEDGER89_OK);
    CHECK_EQ(count_sealed(g.path), 2);
    CHECK_EQ(ledger89_last_index(g.l), 7ul);
    fx_close(&g);

    /* R06/R07: the byte threshold (32 header + 2 * 85-byte batches). */
    CHECK_EQ(fx_open_limits(&g, 202ul, 0ul), LEDGER89_OK);
    CHECK_EQ(append_tag(&g, 1ul, 1ul), LEDGER89_OK);
    CHECK_EQ(append_tag(&g, 2ul, 2ul), LEDGER89_OK);
    CHECK_EQ(count_sealed(g.path), 0);
    CHECK_EQ(append_tag(&g, 3ul, 3ul), LEDGER89_OK);
    CHECK_EQ(count_sealed(g.path), 1);
    CHECK_EQ(ledger89_last_index(g.l), 3ul);
    fx_close(&g);

    /* R08: a record larger than the target occupies its own segment. */
    CHECK_EQ(fx_open_limits(&g, 64ul, 0ul), LEDGER89_OK);
    {
        unsigned char big[100];

        memset(big, 'B', sizeof big);
        CHECK_EQ(fx_append(&g, 1ul, 9ul, big, sizeof big), LEDGER89_OK);
    }
    CHECK_EQ(count_sealed(g.path), 0);
    CHECK_EQ(append_tag(&g, 2ul, 2ul), LEDGER89_OK);
    CHECK_EQ(count_sealed(g.path), 1);
    CHECK_EQ(fx_read(&g, 1ul, &v), LEDGER89_OK);
    CHECK_EQ(v.size, 100u);
    fx_close(&g);

    /* R09: repeated explicit rotations. */
    CHECK_EQ(fx_open(&g), LEDGER89_OK);
    for (i = 1ul; i <= 20ul; ++i)
    {
        CHECK_EQ(append_tag(&g, i, i), LEDGER89_OK);
        CHECK_EQ(ledger89_rotate(g.l), LEDGER89_OK);
    }
    CHECK_EQ(count_sealed(g.path), 20);
    CHECK_EQ(ledger89_last_index(g.l), 20ul);
    fx_close(&g);

    /* R13: iterate across 100 single-record segments. */
    CHECK_EQ(fx_open_limits(&g, 0ul, 1ul), LEDGER89_OK);
    for (i = 1ul; i <= 100ul; ++i)
    {
        CHECK_EQ(append_tag(&g, i, i * 2ul), LEDGER89_OK);
    }
    CHECK_EQ(count_sealed(g.path), 99);
    {
        ledger89_iter *it;
        size_t count;

        it = NULL;
        CHECK_EQ(ledger89_iter_open(g.l, 0ul, 0ul, &it), LEDGER89_OK);
        count = 0u;
        for (;;)
        {
            int rc;

            rc = ledger89_iter_next(it, &v);
            if (rc == LEDGER89_END)
            {
                break;
            }
            CHECK_EQ(rc, LEDGER89_OK);
            if (rc != LEDGER89_OK)
            {
                break;
            }
            ++count;
            CHECK_EQ(v.index, (ledger89_index)count);
        }
        ledger89_iter_close(it);
        CHECK_EQ(count, 100u);
    }
    CHECK_EQ(fx_reopen(&g), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(g.l), 100ul);
    CHECK_EQ(fx_read(&g, 100ul, &v), LEDGER89_OK);
    fx_close(&g);

    TEST_END;
}
