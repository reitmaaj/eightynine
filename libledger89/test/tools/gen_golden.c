/* gen_golden.c - regenerate the committed golden byte fixtures.
 *
 * Builds a deterministic ledger through the POSIX backend with fixed
 * identity entropy and copies the resulting files into test/golden/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "real_io.h"
#include "tmpdir.h"

static const unsigned char fixed_id[16] = {
    0x89u, 0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xABu, 0xCDu,
    0xEFu, 0x10u, 0x32u, 0x54u, 0x76u, 0x98u, 0xBAu, 0xDCu};

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

static void copy_from(const char *dir, const char *name, const char *dst)
{
    char src[160];

    sprintf(src, "%s/%s", dir, name);
    if (copy_file(src, dst) == 0)
    {
        fprintf(stderr, "copy failed: %s\n", src);
    }
}

int main(void)
{
    char dir[64];
    real_io r;
    ledger89 *l;
    unsigned char bin[3];
    ledger89_slice s;

    if (tmpdir_create(dir, sizeof dir) != 0)
    {
        return 1;
    }
    real_io_init(&r);
    real_io_fix_entropy(&r, fixed_id);
    l = NULL;
    if (led89_open_io(&l, dir,
                      LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                          LEDGER89_OPEN_EXCL,
                      real_io_api(&r)) != LEDGER89_OK)
    {
        return 1;
    }
    s.data = "alpha";
    s.size = 5u;
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    bin[0] = 0x01u;
    bin[1] = 0x02u;
    bin[2] = 0x03u;
    s.data = bin;
    s.size = 3u;
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    s.data = NULL;
    s.size = 0u;
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    if (ledger89_rotate(l) != LEDGER89_OK)
    {
        return 1;
    }
    s.data = "delta";
    s.size = 5u;
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    s.data = "zz";
    s.size = 2u;
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        return 1;
    }
    ledger89_close(l);

    copy_from(dir, "CURRENT", "test/golden/golden-current.bin");
    copy_from(dir, "MANIFEST.0000000000000001",
              "test/golden/golden-manifest1.bin");
    copy_from(dir, "MANIFEST.0000000000000002",
              "test/golden/golden-manifest2.bin");
    copy_from(dir, "part.0000000000000001", "test/golden/golden-part1.bin");
    copy_from(dir, "part.0000000000000002", "test/golden/golden-part2.bin");
    printf("golden fixtures regenerated\n");
    return 0;
}
