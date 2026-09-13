/* gen_golden.c - regenerate the frozen on-disk byte fixtures under
 * test/golden/ from a deterministic public-API sequence. Run via
 * `just golden-gen`; the fixtures are committed and verified by
 * test/golden/test_golden.c. Not part of the library. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "fixture.h"

#define SEALED_NAME "00000000000000000001.seg"
#define ACTIVE_NAME "active.seg"
#define SEALED_DST "test/golden/golden-sealed.seg"
#define ACTIVE_DST "test/golden/golden-active.seg"

static const unsigned char payload_alpha[5] = {'a', 'l', 'p', 'h', 'a'};
static const unsigned char payload_binary[4] = {0xDE, 0xAD, 0xBE, 0xEF};
static const unsigned char payload_delta[5] = {'d', 'e', 'l', 't', 'a'};
static const unsigned char payload_zero[1] = {0x00};

static int copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buf[4096];
    size_t n;
    int ok;

    in = fopen(src, "rb");
    if (in == NULL)
    {
        return -1;
    }
    out = fopen(dst, "wb");
    if (out == NULL)
    {
        fclose(in);
        return -1;
    }
    ok = 0;
    for (;;)
    {
        n = fread(buf, 1u, sizeof buf, in);
        if (n > 0u)
        {
            if (fwrite(buf, 1u, n, out) != n)
            {
                break;
            }
        }
        if (n < sizeof buf)
        {
            if (ferror(in) == 0)
            {
                ok = 1;
            }
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0)
    {
        ok = 0;
    }
    return ok != 0 ? 0 : -1;
}

static int copy_from_dir(const char *dir, const char *name, const char *dst)
{
    char src[128];
    size_t n;
    size_t m;

    n = strlen(dir);
    m = strlen(name);
    if (n + m + 2u > sizeof src)
    {
        return -1;
    }
    memcpy(src, dir, n);
    src[n] = '/';
    memcpy(src + n + 1u, name, m + 1u);
    return copy_file(src, dst);
}

static int ensure_dir(const char *path)
{
    if (mkdir(path, 0777) == 0)
    {
        return 0;
    }
    if (errno == EEXIST)
    {
        return 0;
    }
    return -1;
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

int main(void)
{
    fx f;
    int rc;

    if (ensure_dir("test/golden") != 0)
    {
        fprintf(stderr, "gen_golden: cannot create test/golden\n");
        return 1;
    }
    rc = build_ledger(&f);
    if (rc != LEDGER89_OK)
    {
        fprintf(stderr, "gen_golden: ledger build failed (%d)\n", rc);
        return 1;
    }
    if (copy_from_dir(f.path, SEALED_NAME, SEALED_DST) != 0)
    {
        fprintf(stderr, "gen_golden: cannot copy sealed fixture\n");
        return 1;
    }
    if (copy_from_dir(f.path, ACTIVE_NAME, ACTIVE_DST) != 0)
    {
        fprintf(stderr, "gen_golden: cannot copy active fixture\n");
        return 1;
    }
    printf("golden fixtures written\n");
    return 0;
}
