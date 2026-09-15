/* test_read_at.c - exact partial-record reads. */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture.h"
#include "test.h"

#define BIG_BYTES ((size_t)8388608u)
#define BIG_OFFSET ((size_t)1048576u)

static void append_one(fx *f, const void *data, size_t size)
{
    ledger89_slice s;

    s.data = data;
    s.size = size;
    CHECK_EQ(ledger89_appendv(f->l, &s, 1u, NULL), LEDGER89_OK);
}

static void expect_range(fx *f, ledger89_index index, size_t offset,
                         const char *expect, size_t size)
{
    unsigned char buf[16];

    CHECK(size <= sizeof buf);
    memset(buf, 0xA5, sizeof buf);
    CHECK_EQ(ledger89_read_at(f->l, index, offset, buf, size), LEDGER89_OK);
    CHECK(memcmp(buf, expect, size) == 0);
}

static void expect_erange(fx *f, ledger89_index index, size_t offset,
                          size_t size)
{
    unsigned char buf[16];
    size_t i;

    memset(buf, 0xA5, sizeof buf);
    CHECK_EQ(ledger89_read_at(f->l, index, offset, buf, size), LEDGER89_ERANGE);
    for (i = 0u; i < sizeof buf; ++i)
    {
        CHECK_EQ(buf[i], 0xA5u);
    }
}

static void test_ranges(void)
{
    fx f;
    unsigned char buf[16];
    ledger89_slice s;
    ledger89_index i;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "abcdefgh";
    s.size = 8u;
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, &i), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_U64(i, test_u64(1));

    expect_range(&f, i, 0u, "abcdefgh", 8u);
    expect_range(&f, i, 0u, "abc", 3u);
    expect_range(&f, i, 2u, "cde", 3u);
    expect_range(&f, i, 5u, "fgh", 3u);
    expect_range(&f, i, 7u, "h", 1u);

    /* Zero-size reads are valid across [0, size] and touch nothing. */
    CHECK_EQ(ledger89_read_at(f.l, i, 0u, NULL, 0u), LEDGER89_OK);
    CHECK_EQ(ledger89_read_at(f.l, i, 4u, NULL, 0u), LEDGER89_OK);
    CHECK_EQ(ledger89_read_at(f.l, i, 8u, NULL, 0u), LEDGER89_OK);
    memset(buf, 0xA5, sizeof buf);
    CHECK_EQ(ledger89_read_at(f.l, i, 4u, buf, 0u), LEDGER89_OK);
    CHECK_EQ(buf[0], 0xA5u);

    /* Range violations copy nothing. */
    expect_erange(&f, i, 9u, 0u);
    expect_erange(&f, i, 8u, 1u);
    expect_erange(&f, i, 7u, 2u);
    expect_erange(&f, i, (size_t)-1, 1u);
    expect_erange(&f, i, (size_t)-1, 0u);

    /* Argument and index validation. */
    rc = ledger89_read_at(NULL, i, 0u, buf, 1u);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_read_at(f.l, i, 0u, NULL, 1u);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_read_at(f.l, test_u64(0), 0u, buf, 1u);
    CHECK_EQ(rc, LEDGER89_EGONE);
    rc = ledger89_read_at(f.l, test_u64(2), 0u, buf, 1u);
    CHECK_EQ(rc, LEDGER89_ENOENT);
    fx_close(&f);
}

static void state_equal(const ledger89_state *a, const ledger89_state *b)
{
    CHECK(memcmp(a->id.bytes, b->id.bytes, sizeof a->id.bytes) == 0);
    CHECK_U64(a->revision, b->revision);
    CHECK_U64(a->first, b->first);
    CHECK_U64(a->stable_end, b->stable_end);
    CHECK_U64(a->end, b->end);
}

static void test_state_unchanged(void)
{
    fx f;
    ledger89_state before;
    ledger89_state after;
    unsigned char buf[8];

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_one(&f, "abcdefgh", 8u);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(f.l, &before), LEDGER89_OK);

    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 8u), LEDGER89_OK);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 9u, buf, 1u), LEDGER89_ERANGE);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(2), 0u, buf, 1u), LEDGER89_ENOENT);
    CHECK_EQ(ledger89_get_state(f.l, &after), LEDGER89_OK);
    state_equal(&before, &after);
    fx_close(&f);
}

static void test_stable_unstable(void)
{
    fx f;
    unsigned char buf[8];

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_one(&f, "AAA", 3u);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    append_one(&f, "BBB", 3u);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 3u), LEDGER89_OK);
    CHECK(memcmp(buf, "AAA", 3u) == 0);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(2), 0u, buf, 3u), LEDGER89_OK);
    CHECK(memcmp(buf, "BBB", 3u) == 0);

    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 3u), LEDGER89_OK);
    CHECK(memcmp(buf, "AAA", 3u) == 0);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(2), 0u, buf, 3u), LEDGER89_ENOENT);
    fx_close(&f);
}

static void test_truncate(void)
{
    fx f;
    unsigned char buf[4];

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_one(&f, "A", 1u);
    append_one(&f, "B", 1u);
    append_one(&f, "C", 1u);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_truncate_from(f.l, test_u64(3)), LEDGER89_OK);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 1u), LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'A');
    CHECK_EQ(ledger89_read_at(f.l, test_u64(2), 0u, buf, 1u), LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'B');
    CHECK_EQ(ledger89_read_at(f.l, test_u64(3), 0u, buf, 1u), LEDGER89_ENOENT);
    append_one(&f, "Z", 1u);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(3), 0u, buf, 1u), LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'Z');
    fx_close(&f);
}

static void test_prune(void)
{
    fx f;
    unsigned char buf[4];
    ledger89_index actual;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_one(&f, "a", 1u);
    append_one(&f, "b", 1u);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f.l), LEDGER89_OK);
    append_one(&f, "c", 1u);
    append_one(&f, "d", 1u);
    append_one(&f, "e", 1u);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_prune_before(f.l, test_u64(3), &actual), LEDGER89_OK);
    CHECK_U64(actual, test_u64(3));
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 1u), LEDGER89_EGONE);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(3), 0u, buf, 1u), LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'c');
    fx_close(&f);
}

static void fill_part(fx *f, unsigned long first)
{
    unsigned long n;
    char rec[32];

    for (n = first; n < first + 3ul; ++n)
    {
        sprintf(rec, "rec%05lu", n);
        append_one(f, rec, 8u);
    }
    CHECK_EQ(ledger89_sync(f->l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(f->l), LEDGER89_OK);
}

static void test_rotation(void)
{
    fx f;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    fill_part(&f, 1ul);
    fill_part(&f, 4ul);
    fill_part(&f, 7ul);

    /* Records in every part, including the last record before a boundary. */
    expect_range(&f, test_u64(1), 0u, "rec00001", 8u);
    expect_range(&f, test_u64(3), 0u, "rec00003", 8u);
    expect_range(&f, test_u64(4), 0u, "rec00004", 8u);
    expect_range(&f, test_u64(6), 0u, "rec00006", 8u);
    expect_range(&f, test_u64(7), 0u, "rec00007", 8u);
    expect_range(&f, test_u64(9), 3u, "00009", 5u);
    CHECK_EQ(ledger89_read_at(f.l, test_u64(9), 8u, NULL, 0u), LEDGER89_OK);
    fx_close(&f);
}

static void test_large(void)
{
    fx f;
    unsigned char *big;
    unsigned char buf[16];
    ledger89_slice s;
    size_t i;

    big = (unsigned char *)malloc(BIG_BYTES);
    CHECK(big != NULL);
    if (big == NULL)
    {
        return;
    }
    for (i = 0u; i < BIG_BYTES; ++i)
    {
        big[i] = (unsigned char)(i & 0xffu);
    }
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = big;
    s.size = BIG_BYTES;
    CHECK_EQ(ledger89_appendv(f.l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);

    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 8u), LEDGER89_OK);
    for (i = 0u; i < 8u; ++i)
    {
        CHECK_EQ(buf[i], (unsigned char)i);
    }
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), BIG_OFFSET, buf, 16u),
             LEDGER89_OK);
    for (i = 0u; i < 16u; ++i)
    {
        CHECK_EQ(buf[i], (unsigned char)((BIG_OFFSET + i) & 0xffu));
    }
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), BIG_BYTES - 8u, buf, 8u),
             LEDGER89_OK);
    for (i = 0u; i < 8u; ++i)
    {
        CHECK_EQ(buf[i], (unsigned char)((BIG_BYTES - 8u + i) & 0xffu));
    }
    free(big);
    fx_close(&f);
}

static int find_part_path(const fx *f, char *out, size_t cap)
{
    DIR *d;
    struct dirent *e;
    int found;

    d = opendir(f->path);
    if (d == NULL)
    {
        return 0;
    }
    found = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strncmp(e->d_name, "part.", 5u) == 0)
        {
            sprintf(out, "%s/%s", f->path, e->d_name);
            (void)cap;
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

static unsigned char *slurp(const char *path, size_t *len_out)
{
    FILE *fp;
    unsigned char *buf;
    long n;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return NULL;
    }
    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        fclose(fp);
        return NULL;
    }
    n = ftell(fp);
    if (n <= 0L)
    {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    buf = (unsigned char *)malloc((size_t)n);
    if (buf == NULL)
    {
        fclose(fp);
        return NULL;
    }
    got = fread(buf, 1u, (size_t)n, fp);
    fclose(fp);
    if (got != (size_t)n)
    {
        free(buf);
        return NULL;
    }
    *len_out = got;
    return buf;
}

static long find_bytes(const unsigned char *hay, size_t hay_len,
                       const char *needle, size_t needle_len)
{
    size_t i;

    if (needle_len == 0u || hay_len < needle_len)
    {
        return -1L;
    }
    for (i = 0u; i + needle_len <= hay_len; ++i)
    {
        if (memcmp(hay + i, needle, needle_len) == 0)
        {
            return (long)i;
        }
    }
    return -1L;
}

static int flip_byte(const char *path, size_t offset)
{
    FILE *fp;
    unsigned char b;
    int ok;

    fp = fopen(path, "r+b");
    if (fp == NULL)
    {
        return 0;
    }
    ok = 0;
    if (fseek(fp, (long)offset, SEEK_SET) == 0 && fread(&b, 1u, 1u, fp) == 1u)
    {
        b = (unsigned char)(b ^ 0xFFu);
        if (fseek(fp, (long)offset, SEEK_SET) == 0 &&
            fwrite(&b, 1u, 1u, fp) == 1u)
        {
            ok = 1;
        }
    }
    fclose(fp);
    return ok;
}

static void corrupt_at(const char *needle, long delta)
{
    fx f;
    char path[96];
    unsigned char payload[4096];
    unsigned char *file;
    unsigned char buf[8];
    size_t len;
    long pos;

    memset(payload, 0x5A, sizeof payload);
    memcpy(payload, "HEADER00", 8u);
    memcpy(payload + 1000u, "NEEDLE42", 8u);
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    append_one(&f, payload, sizeof payload);
    CHECK_EQ(ledger89_sync(f.l, NULL), LEDGER89_OK);
    CHECK(find_part_path(&f, path, sizeof path));
    file = slurp(path, &len);
    CHECK(file != NULL);
    if (file == NULL)
    {
        fx_close(&f);
        return;
    }
    pos = find_bytes(file, len, needle, 8u);
    CHECK(pos >= 0L);
    free(file);
    if (pos < 0L)
    {
        fx_close(&f);
        return;
    }
    CHECK(flip_byte(path, (size_t)(pos + delta)));
    CHECK_EQ(ledger89_read_at(f.l, test_u64(1), 0u, buf, 8u),
             LEDGER89_ECORRUPT);
    fx_close(&f);
}

static void test_corruption(void)
{
    corrupt_at("NEEDLE42", 0L);    /* outside the requested slice */
    corrupt_at("HEADER00", 0L);    /* inside the requested slice */
    corrupt_at("HEADER00", 4096L); /* record checksum */
    corrupt_at("HEADER00", -4L);   /* record framing length */
}

int main(void)
{
    test_ranges();
    test_state_unchanged();
    test_stable_unstable();
    test_truncate();
    test_prune();
    test_rotation();
    test_large();
    test_corruption();
    TEST_END;
}
