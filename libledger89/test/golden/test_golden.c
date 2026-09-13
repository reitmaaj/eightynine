/* test_golden.c - GF01..GF03: frozen on-disk bytes encode and decode. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#include "fixture.h"
#include "tmpdir.h"

#define SEALED_NAME "00000000000000000001.seg"
#define ACTIVE_NAME "active.seg"
#define SEALED_FIX "test/golden/golden-sealed.seg"
#define ACTIVE_FIX "test/golden/golden-active.seg"

static const unsigned char payload_alpha[5] = {'a', 'l', 'p', 'h', 'a'};
static const unsigned char payload_binary[4] = {0xDE, 0xAD, 0xBE, 0xEF};
static const unsigned char payload_delta[5] = {'d', 'e', 'l', 't', 'a'};
static const unsigned char payload_zero[1] = {0x00};

static unsigned char *read_all(const char *path, size_t *size)
{
    FILE *in;
    long len;
    unsigned char *buf;
    size_t n;

    in = fopen(path, "rb");
    if (in == NULL)
    {
        return NULL;
    }
    if (fseek(in, 0L, SEEK_END) != 0)
    {
        fclose(in);
        return NULL;
    }
    len = ftell(in);
    if (len < 0L)
    {
        fclose(in);
        return NULL;
    }
    if (fseek(in, 0L, SEEK_SET) != 0)
    {
        fclose(in);
        return NULL;
    }
    buf = (unsigned char *)malloc((size_t)len + 1u);
    if (buf == NULL)
    {
        fclose(in);
        return NULL;
    }
    n = fread(buf, 1u, (size_t)len, in);
    fclose(in);
    if (n != (size_t)len)
    {
        free(buf);
        return NULL;
    }
    *size = n;
    return buf;
}

static int write_all(const char *path, const unsigned char *buf, size_t size)
{
    FILE *out;
    size_t n;

    out = fopen(path, "wb");
    if (out == NULL)
    {
        return -1;
    }
    n = fwrite(buf, 1u, size, out);
    if (fclose(out) != 0)
    {
        return -1;
    }
    return n == size ? 0 : -1;
}

static int copy_file(const char *src, const char *dst)
{
    unsigned char *buf;
    size_t size;
    int rc;

    buf = read_all(src, &size);
    if (buf == NULL)
    {
        return -1;
    }
    rc = write_all(dst, buf, size);
    free(buf);
    return rc;
}

static int join_path(char *out, size_t cap, const char *dir, const char *name)
{
    size_t n;
    size_t m;

    n = strlen(dir);
    m = strlen(name);
    if (n + m + 2u > cap)
    {
        return -1;
    }
    memcpy(out, dir, n);
    out[n] = '/';
    memcpy(out + n + 1u, name, m + 1u);
    return 0;
}

static int copy_into(const char *src, const char *dir, const char *name)
{
    char dst[128];

    if (join_path(dst, sizeof dst, dir, name) != 0)
    {
        return -1;
    }
    return copy_file(src, dst);
}

static int same_file(const char *a, const char *b)
{
    unsigned char *ba;
    unsigned char *bb;
    size_t na;
    size_t nb;
    int same;

    ba = read_all(a, &na);
    bb = read_all(b, &nb);
    same = 0;
    if (ba != NULL && bb != NULL && na == nb)
    {
        if (memcmp(ba, bb, na) == 0)
        {
            same = 1;
        }
    }
    free(ba);
    free(bb);
    return same;
}

static long find_bytes(const unsigned char *hay, size_t n,
                       const unsigned char *needle, size_t m)
{
    size_t i;
    size_t j;

    if (m == 0u)
    {
        return -1L;
    }
    for (i = 0u; i + m <= n; ++i)
    {
        j = 0u;
        while (j < m && hay[i + j] == needle[j])
        {
            ++j;
        }
        if (j == m)
        {
            return (long)i;
        }
    }
    return -1L;
}

static int build_ledger(fx *f)
{
    ledger89_record batch[3];
    int rc;

    rc = fx_open(f);
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    fx_record(&batch[0], 1ul, 1001ul, payload_alpha, sizeof payload_alpha);
    fx_record(&batch[1], 2ul, 0x01020304ul, payload_binary,
              sizeof payload_binary);
    fx_record(&batch[2], 3ul, 0ul, NULL, 0u);
    rc = ledger89_append(f->l, batch, 3u);
    if (rc == LEDGER89_OK)
    {
        rc = ledger89_sync(f->l);
    }
    if (rc == LEDGER89_OK)
    {
        rc = ledger89_rotate(f->l);
    }
    if (rc != LEDGER89_OK)
    {
        return rc;
    }
    fx_record(&batch[0], 4ul, 0xCAFEBABEul, payload_delta,
              sizeof payload_delta);
    fx_record(&batch[1], 5ul, 2ul, payload_zero, sizeof payload_zero);
    rc = ledger89_append(f->l, batch, 2u);
    if (rc == LEDGER89_OK)
    {
        rc = ledger89_sync(f->l);
    }
    fx_close(f);
    return rc;
}

static void check_record(ledger89 *l, ledger89_index index, unsigned long tag,
                         const void *data, size_t size)
{
    ledger89_view v;

    CHECK_EQ(ledger89_read(l, index, &v), LEDGER89_OK);
    CHECK_EQ(v.index, index);
    CHECK_EQ(v.tag, tag);
    CHECK_EQ(v.size, size);
    if (size > 0u)
    {
        CHECK_EQ(memcmp(v.data, data, size), 0);
    }
}

int main(void)
{
    fx f;
    ledger89 *l;
    ledger89_config config;
    ledger89_iter *it;
    ledger89_view v;
    unsigned char *buf;
    size_t size;
    long at;
    char path[128];
    char dir[64];
    size_t i;

    /* GF01: encode freeze. */
    CHECK_EQ(build_ledger(&f), LEDGER89_OK);
    CHECK_EQ(join_path(path, sizeof path, f.path, SEALED_NAME), 0);
    CHECK_EQ(same_file(path, SEALED_FIX), 1);
    CHECK_EQ(join_path(path, sizeof path, f.path, ACTIVE_NAME), 0);
    CHECK_EQ(same_file(path, ACTIVE_FIX), 1);

    /* GF02: decode freeze. */
    CHECK_EQ(tmpdir_create(dir, sizeof dir), 0);
    CHECK_EQ(copy_into(SEALED_FIX, dir, SEALED_NAME), 0);
    CHECK_EQ(copy_into(ACTIVE_FIX, dir, ACTIVE_NAME), 0);
    memset(&config, 0, sizeof config);
    config.path = dir;
    l = NULL;
    CHECK_EQ(ledger89_open(&l, &config), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 1ul);
    CHECK_EQ(ledger89_last_index(l), 5ul);
    check_record(l, 1ul, 1001ul, payload_alpha, sizeof payload_alpha);
    check_record(l, 2ul, 0x01020304ul, payload_binary, sizeof payload_binary);
    check_record(l, 3ul, 0ul, NULL, 0u);
    check_record(l, 4ul, 0xCAFEBABEul, payload_delta, sizeof payload_delta);
    check_record(l, 5ul, 2ul, payload_zero, sizeof payload_zero);
    it = NULL;
    CHECK_EQ(ledger89_iter_open(l, 0ul, 0ul, &it), LEDGER89_OK);
    for (i = 0u; i < 5u; ++i)
    {
        CHECK_EQ(ledger89_iter_next(it, &v), LEDGER89_OK);
        CHECK_EQ(v.index, (ledger89_index)(i + 1u));
    }
    CHECK_EQ(ledger89_iter_next(it, &v), LEDGER89_END);
    ledger89_iter_close(it);
    ledger89_close(l);

    /* GF03: a byte-flipped fixture must not decode. */
    CHECK_EQ(tmpdir_create(dir, sizeof dir), 0);
    CHECK_EQ(copy_into(SEALED_FIX, dir, SEALED_NAME), 0);
    CHECK_EQ(copy_into(ACTIVE_FIX, dir, ACTIVE_NAME), 0);
    CHECK_EQ(join_path(path, sizeof path, dir, SEALED_NAME), 0);
    buf = read_all(path, &size);
    CHECK(buf != NULL);
    at = find_bytes(buf, size, payload_binary, sizeof payload_binary);
    CHECK(at >= 0L);
    if (at >= 0L)
    {
        buf[(size_t)at] = (unsigned char)(buf[(size_t)at] ^ 0xFFu);
        CHECK_EQ(write_all(path, buf, size), 0);
    }
    free(buf);
    memset(&config, 0, sizeof config);
    config.path = dir;
    l = NULL;
    CHECK_EQ(ledger89_open(&l, &config), LEDGER89_OK);
    CHECK_EQ(ledger89_read(l, 2ul, &v), LEDGER89_ERR_CORRUPT);
    ledger89_close(l);

    TEST_END;
}
