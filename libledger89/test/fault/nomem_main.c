/* nomem_main.c - allocation failure sweep over the public API.
 *
 * Linked with -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc. Every
 * operation that allocates must fail with NOMEM, leave the logical ledger
 * unchanged (or fault the handle), and succeed on a later attempt. */

#include <string.h>

#include "test.h"

#include "nomem_shim.h"
#include "tmpdir.h"

static int open_ledger_records(const char *path, unsigned long records,
                               ledger89 **l)
{
    ledger89_config cfg;

    memset(&cfg, 0, sizeof cfg);
    cfg.path = path;
    cfg.max_segment_bytes = 4096ul;
    cfg.max_segment_records = records;
    *l = NULL;
    return ledger89_open(l, &cfg);
}

static int open_ledger(const char *path, ledger89 **l)
{
    return open_ledger_records(path, 2ul, l);
}

static int make_ledger(const char *path, unsigned long records,
                       ledger89_index count)
{
    ledger89 *l;
    ledger89_record r;
    unsigned char v;
    ledger89_index i;

    if (open_ledger_records(path, records, &l) != LEDGER89_OK)
    {
        return 0;
    }
    for (i = 1ul; i <= count; ++i)
    {
        v = (unsigned char)('a' + (int)(i % 26ul));
        r.index = i;
        r.tag = (unsigned long)i;
        r.data = &v;
        r.size = 1u;
        if (ledger89_append(l, &r, 1u) != LEDGER89_OK)
        {
            ledger89_close(l);
            return 0;
        }
    }
    if (ledger89_sync(l) != LEDGER89_OK)
    {
        ledger89_close(l);
        return 0;
    }
    ledger89_close(l);
    return 1;
}

static void sweep_open(const char *path)
{
    int i;

    for (i = 0;; ++i)
    {
        ledger89 *l;
        int rc;

        l = NULL;
        nomem_arm(i);
        rc = open_ledger(path, &l);
        nomem_disarm();
        if (rc == LEDGER89_ERR_NOMEM)
        {
            CHECK(l == NULL);
            continue;
        }
        CHECK_EQ(rc, LEDGER89_OK);
        CHECK(l != NULL);
        if (l != NULL)
        {
            ledger89_close(l);
        }
        break;
    }
    CHECK(i > 0);
}

static void sweep_read(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        ledger89_view v;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 2ul, 3ul) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_read(l, 1ul, &v);
        nomem_disarm();
        if (rc == LEDGER89_ERR_NOMEM)
        {
            ledger89_close(l);
            continue;
        }
        CHECK_EQ(rc, LEDGER89_OK);
        ledger89_close(l);
        break;
    }
    CHECK(i > 0);
}

static void sweep_iter_open(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        ledger89_iter *it;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 2ul, 3ul) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        it = NULL;
        nomem_arm(i);
        rc = ledger89_iter_open(l, 0ul, 0ul, &it);
        nomem_disarm();
        if (rc == LEDGER89_ERR_NOMEM)
        {
            CHECK(it == NULL);
            ledger89_close(l);
            continue;
        }
        CHECK_EQ(rc, LEDGER89_OK);
        ledger89_iter_close(it);
        ledger89_close(l);
        break;
    }
    CHECK(i > 0);
}

static void sweep_iter_next(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        ledger89_iter *it;
        ledger89_view v;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 2ul, 3ul) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        it = NULL;
        CHECK_EQ(ledger89_iter_open(l, 0ul, 0ul, &it), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_iter_next(it, &v);
        nomem_disarm();
        if (rc == LEDGER89_ERR_NOMEM)
        {
            ledger89_iter_close(it);
            ledger89_close(l);
            continue;
        }
        CHECK_EQ(rc, LEDGER89_OK);
        ledger89_iter_close(it);
        ledger89_close(l);
        break;
    }
    CHECK(i > 0);
}

static void sweep_rotate(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 0ul, 4ul) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_rotate(l);
        nomem_disarm();
        if (rc == LEDGER89_ERR_NOMEM)
        {
            CHECK_EQ(ledger89_sync(l), LEDGER89_ERR_FAULTED);
            ledger89_close(l);
            continue;
        }
        CHECK_EQ(rc, LEDGER89_OK);
        ledger89_close(l);
        break;
    }
    CHECK(i > 0);
}

int main(void)
{
    char path[64];

    CHECK(tmpdir_create(path, sizeof path) == 0);
    sweep_open(path);
    sweep_read();
    sweep_iter_open();
    sweep_iter_next();
    sweep_rotate();

    TEST_END;
}
