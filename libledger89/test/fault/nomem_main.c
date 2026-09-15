/* nomem_main.c - allocation failure sweep over the public API.
 *
 * Linked with -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc. Every
 * operation that allocates must fail cleanly, leave the ledger recoverable,
 * and succeed on a later attempt. */

#include <string.h>

#include "test.h"

#include "nomem_shim.h"
#include "tmpdir.h"

static int open_ledger(const char *path, ledger89 **l)
{
    *l = NULL;
    return ledger89_open(l, path, LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE);
}

static int make_ledger(const char *path, unsigned long count, int rotate_after)
{
    ledger89 *l;
    unsigned long i;

    if (open_ledger(path, &l) != LEDGER89_OK)
    {
        return 0;
    }
    for (i = 1ul; i <= count; ++i)
    {
        unsigned char v;
        ledger89_slice s;

        v = (unsigned char)('a' + (int)(i % 26ul));
        s.data = &v;
        s.size = 1u;
        if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
        {
            ledger89_close(l);
            return 0;
        }
        if (rotate_after != 0 && i == (count / 2ul))
        {
            if (ledger89_sync(l, NULL) != LEDGER89_OK)
            {
                ledger89_close(l);
                return 0;
            }
            if (ledger89_rotate(l) != LEDGER89_OK)
            {
                ledger89_close(l);
                return 0;
            }
        }
    }
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        ledger89_close(l);
        return 0;
    }
    ledger89_close(l);
    return 1;
}

static void check_recoverable(const char *path)
{
    ledger89 *l;
    ledger89_state st;
    ledger89_slice s;
    unsigned char v;

    v = 'q';
    s.data = &v;
    s.size = 1u;
    l = NULL;
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    if (l == NULL)
    {
        return;
    }
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.stable_end, st.end);
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    ledger89_close(l);
}

static void sweep_open(const char *path)
{
    int i;

    for (i = 0;; ++i)
    {
        ledger89 *l;
        int rc;

        int fired;

        l = NULL;
        nomem_arm(i);
        rc = open_ledger(path, &l);
        fired = nomem_fired();
        nomem_disarm();
        if (fired != 0)
        {
            if (rc != LEDGER89_ENOMEM)
            {
                fprintf(stderr, "probe open i=%d rc=%d\n", i, rc);
            }
            CHECK_EQ(rc, LEDGER89_ENOMEM);
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

static void sweep_append(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        ledger89_slice s;
        unsigned char v;
        int rc;

        v = 'z';
        s.data = &v;
        s.size = 1u;
        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 64ul, 0) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_appendv(l, &s, 1u, NULL);
        {
            int fired;

            fired = nomem_fired();
            nomem_disarm();
            ledger89_close(l);
            if (fired == 0)
            {
                CHECK_EQ(rc, LEDGER89_OK);
                break;
            }
            if (rc != LEDGER89_ENOMEM && rc != LEDGER89_EPOISONED)
            {
                fprintf(stderr, "probe append i=%d rc=%d\n", i, rc);
            }
            CHECK(rc == LEDGER89_ENOMEM || rc == LEDGER89_EPOISONED);
        }
        check_recoverable(path);
        continue;
    }
    CHECK(i > 0);
}

static void sweep_truncate(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 6ul, 1) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_truncate_from(l, test_u64(3));
        {
            int fired;

            fired = nomem_fired();
            nomem_disarm();
            ledger89_close(l);
            if (fired == 0)
            {
                CHECK_EQ(rc, LEDGER89_OK);
                break;
            }
            if (rc != LEDGER89_ENOMEM && rc != LEDGER89_EPOISONED)
            {
                fprintf(stderr, "probe truncate i=%d rc=%d\n", i, rc);
            }
            CHECK(rc == LEDGER89_ENOMEM || rc == LEDGER89_EPOISONED);
        }
        check_recoverable(path);
        continue;
    }
    CHECK(i > 0);
}

static void sweep_prune(void)
{
    int i;

    for (i = 0;; ++i)
    {
        char path[64];
        ledger89 *l;
        ledger89_index actual;
        int rc;

        CHECK(tmpdir_create(path, sizeof path) == 0);
        CHECK(make_ledger(path, 6ul, 1) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_prune_before(l, test_u64(4), &actual);
        {
            int fired;

            fired = nomem_fired();
            nomem_disarm();
            ledger89_close(l);
            if (fired == 0)
            {
                CHECK_EQ(rc, LEDGER89_OK);
                break;
            }
            if (rc != LEDGER89_ENOMEM && rc != LEDGER89_EPOISONED)
            {
                fprintf(stderr, "probe prune i=%d rc=%d\n", i, rc);
            }
            CHECK(rc == LEDGER89_ENOMEM || rc == LEDGER89_EPOISONED);
        }
        check_recoverable(path);
        continue;
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
        CHECK(make_ledger(path, 4ul, 0) != 0);
        CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
        nomem_arm(i);
        rc = ledger89_rotate(l);
        {
            int fired;

            fired = nomem_fired();
            nomem_disarm();
            ledger89_close(l);
            if (fired == 0)
            {
                CHECK_EQ(rc, LEDGER89_OK);
                break;
            }
            if (rc != LEDGER89_ENOMEM && rc != LEDGER89_EPOISONED)
            {
                fprintf(stderr, "probe rotate i=%d rc=%d\n", i, rc);
            }
            CHECK(rc == LEDGER89_ENOMEM || rc == LEDGER89_EPOISONED);
        }
        check_recoverable(path);
        continue;
    }
    CHECK(i > 0);
}

static int make_wide_ledger(const char *path)
{
    ledger89 *l;
    ledger89_slice s;

    s.data = "abcdefgh";
    s.size = 8u;
    l = NULL;
    if (open_ledger(path, &l) != LEDGER89_OK)
    {
        return 0;
    }
    if (ledger89_appendv(l, &s, 1u, NULL) != LEDGER89_OK)
    {
        ledger89_close(l);
        return 0;
    }
    if (ledger89_sync(l, NULL) != LEDGER89_OK)
    {
        ledger89_close(l);
        return 0;
    }
    ledger89_close(l);
    return 1;
}

static void sweep_read_at(void)
{
    char path[64];
    ledger89 *l;
    unsigned char buf[2];
    int rc;
    int fired;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_wide_ledger(path) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);

    /* Open preallocates the scratch buffer, so a partial read that must
     * checksum the record tail performs no allocation of its own. */
    nomem_arm(0);
    rc = ledger89_read_at(l, test_u64(1), 0u, buf, 1u);
    fired = nomem_fired();
    nomem_disarm();
    CHECK_EQ(fired, 0);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(buf[0], (unsigned char)'a');
    ledger89_close(l);
}

int main(void)
{
    char path[64];

    CHECK(tmpdir_create(path, sizeof path) == 0);
    sweep_open(path);
    sweep_append();
    sweep_truncate();
    sweep_prune();
    sweep_rotate();
    sweep_read_at();

    TEST_END;
}
