/* test_golden.c - frozen byte fixtures for format version 2. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "real_io.h"
#include "test.h"
#include "tmpdir.h"

static const unsigned char fixed_id[16] = {
    0x89u, 0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xABu, 0xCDu,
    0xEFu, 0x10u, 0x32u, 0x54u, 0x76u, 0x98u, 0xBAu, 0xDCu};

static const char *golden_files[5] = {
    "golden-current.bin", "golden-manifest1.bin", "golden-manifest2.bin",
    "golden-part1.bin", "golden-part2.bin"};

static const char *ledger_names[5] = {
    "CURRENT", "MANIFEST.0000000000000001", "MANIFEST.0000000000000002",
    "part.0000000000000001", "part.0000000000000002"};

static int copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buf[4096];
    size_t n;

    in = fopen(src, "rb");
    if (in == NULL)
    {
        return 0;
    }
    out = fopen(dst, "wb");
    if (out == NULL)
    {
        fclose(in);
        return 0;
    }
    while ((n = fread(buf, 1u, sizeof buf, in)) > 0u)
    {
        if (fwrite(buf, 1u, n, out) != n)
        {
            fclose(in);
            fclose(out);
            return 0;
        }
    }
    fclose(in);
    if (fclose(out) != 0)
    {
        return 0;
    }
    return 1;
}

static int file_equal(const char *a, const char *b)
{
    FILE *fa;
    FILE *fb;
    unsigned char ba[4096];
    unsigned char bb[4096];
    size_t na;
    size_t nb;

    fa = fopen(a, "rb");
    fb = fopen(b, "rb");
    if (fa == NULL || fb == NULL)
    {
        if (fa != NULL)
        {
            fclose(fa);
        }
        if (fb != NULL)
        {
            fclose(fb);
        }
        return 0;
    }
    for (;;)
    {
        na = fread(ba, 1u, sizeof ba, fa);
        nb = fread(bb, 1u, sizeof bb, fb);
        if (na != nb || memcmp(ba, bb, na) != 0)
        {
            fclose(fa);
            fclose(fb);
            return 0;
        }
        if (na < sizeof ba)
        {
            break;
        }
    }
    fclose(fa);
    fclose(fb);
    return 1;
}

static void build_ledger(const char *dir, real_io *r)
{
    ledger89 *l;
    unsigned char bin[3];
    ledger89_slice s;

    real_io_init(r);
    real_io_fix_entropy(r, fixed_id);
    l = NULL;
    CHECK_EQ(led89_open_io(&l, dir,
                           LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                               LEDGER89_OPEN_EXCL,
                           real_io_api(r)),
             LEDGER89_OK);
    if (l == NULL)
    {
        return;
    }
    s.data = "alpha";
    s.size = 5u;
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    bin[0] = 0x01u;
    bin[1] = 0x02u;
    bin[2] = 0x03u;
    s.data = bin;
    s.size = 3u;
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    s.data = NULL;
    s.size = 0u;
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    s.data = "delta";
    s.size = 5u;
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    s.data = "zz";
    s.size = 2u;
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    ledger89_close(l);
}

static void test_encode_freeze(void)
{
    char dir[64];
    real_io r;
    size_t i;

    CHECK(tmpdir_create(dir, sizeof dir) == 0);
    build_ledger(dir, &r);
    for (i = 0u; i < 5u; ++i)
    {
        char generated[160];
        char fixture[160];

        sprintf(generated, "%s/%s", dir, ledger_names[i]);
        sprintf(fixture, "test/golden/%s", golden_files[i]);
        CHECK(file_equal(generated, fixture) != 0);
    }
}

static void test_decode_freeze(void)
{
    char dir[64];
    ledger89 *l;
    ledger89_state st;
    unsigned char buf[8];
    size_t size;
    size_t i;
    int rc;

    CHECK(tmpdir_create(dir, sizeof dir) == 0);
    for (i = 0u; i < 5u; ++i)
    {
        char src[160];
        char dst[160];

        sprintf(src, "test/golden/%s", golden_files[i]);
        sprintf(dst, "%s/%s", dir, ledger_names[i]);
        CHECK(copy_file(src, dst) != 0);
    }
    l = NULL;
    rc = ledger89_open(&l, dir, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_OK);
    if (l == NULL)
    {
        return;
    }
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.end, test_u64(6));
    CHECK_U64(st.stable_end, test_u64(6));
    CHECK_EQ(st.revision.lo, 0u);
    CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 5u);
    CHECK(memcmp(buf, "alpha", 5u) == 0);
    CHECK_EQ(ledger89_read(l, test_u64(2), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 3u);
    CHECK_EQ(buf[0], 0x01u);
    CHECK_EQ(ledger89_read(l, test_u64(3), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 0u);
    CHECK_EQ(ledger89_read(l, test_u64(4), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 5u);
    CHECK(memcmp(buf, "delta", 5u) == 0);
    CHECK_EQ(ledger89_read(l, test_u64(5), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 2u);
    CHECK(memcmp(buf, "zz", 2u) == 0);
    ledger89_close(l);
}

static void test_corrupt_fixture(void)
{
    char dir[64];
    char path[160];
    FILE *f;
    unsigned char data[512];
    size_t size;
    size_t i;
    int flipped;
    ledger89 *l;
    unsigned char buf[8];
    size_t out_size;
    int rc;

    CHECK(tmpdir_create(dir, sizeof dir) == 0);
    for (i = 0u; i < 5u; ++i)
    {
        char src[160];
        char dst[160];

        sprintf(src, "test/golden/%s", golden_files[i]);
        sprintf(dst, "%s/%s", dir, ledger_names[i]);
        CHECK(copy_file(src, dst) != 0);
    }
    sprintf(path, "%s/%s", dir, "part.0000000000000002");
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f == NULL)
    {
        return;
    }
    size = fread(data, 1u, sizeof data, f);
    fclose(f);
    flipped = 0;
    for (i = 0u; i + 5u <= size; ++i)
    {
        if (memcmp(data + i, "delta", 5u) == 0)
        {
            data[i] = (unsigned char)'D';
            flipped = 1;
            break;
        }
    }
    CHECK_EQ(flipped, 1);
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL)
    {
        return;
    }
    CHECK_EQ(fwrite(data, 1u, size, f), size);
    fclose(f);

    l = NULL;
    rc = ledger89_open(&l, dir, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_OK);
    if (l == NULL)
    {
        return;
    }
    rc = ledger89_read(l, test_u64(4), buf, sizeof buf, &out_size);
    CHECK_EQ(rc, LEDGER89_ECORRUPT);
    ledger89_close(l);
}

int main(void)
{
    test_encode_freeze();
    test_decode_freeze();
    test_corrupt_fixture();
    TEST_END;
}
