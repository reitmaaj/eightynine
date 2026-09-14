/* test_open.c - open/create/close, flags, locking, and identity. */

#include "fixture.h"
#include "test.h"
#include "tmpdir.h"

static void test_create_and_reopen(void)
{
    fx f;
    ledger89_state st;
    ledger89 *l2;
    ledger89_id id;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    rc = ledger89_get_state(f.l, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_U64(st.first, test_u64(1));
    CHECK_U64(st.stable_end, test_u64(1));
    CHECK_U64(st.end, test_u64(1));
    CHECK_EQ(st.revision.hi, 0u);
    CHECK_EQ(st.revision.lo, 0u);
    id = st.id;
    fx_close(&f);

    l2 = NULL;
    rc = ledger89_open(&l2, f.path, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_get_state(l2, &st);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK(memcmp(st.id.bytes, id.bytes, 16u) == 0);
    ledger89_close(l2);
}

static void test_open_errors(void)
{
    fx f;
    ledger89 *l;
    char missing[64];
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);

    l = NULL;
    rc = ledger89_open(&l, f.path, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_EBUSY);
    CHECK(l == NULL);

    l = NULL;
    rc = ledger89_open(&l, f.path, LEDGER89_OPEN_RDONLY);
    CHECK_EQ(rc, LEDGER89_EBUSY);
    CHECK(l == NULL);

    l = NULL;
    rc = ledger89_open(&l, f.path,
                       LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                           LEDGER89_OPEN_EXCL);
    CHECK_EQ(rc, LEDGER89_EBUSY);
    CHECK(l == NULL);

    rc = ledger89_open(&l, f.path, 0ul);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_open(&l, f.path, LEDGER89_OPEN_RDWR | LEDGER89_OPEN_RDONLY);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_open(&l, f.path, LEDGER89_OPEN_RDONLY | LEDGER89_OPEN_CREATE);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_open(&l, f.path, LEDGER89_OPEN_RDWR | LEDGER89_OPEN_EXCL);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_open(&l, NULL, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    rc = ledger89_open(NULL, f.path, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_EINVAL);
    fx_close(&f);

    /* CREATE|EXCL on an existing ledger fails once it is not busy. */
    l = NULL;
    rc = ledger89_open(&l, f.path,
                       LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                           LEDGER89_OPEN_EXCL);
    CHECK_EQ(rc, LEDGER89_EEXIST);
    CHECK(l == NULL);

    if (tmpdir_create(missing, sizeof missing) != 0)
    {
        return;
    }
    /* The directory exists but holds no ledger. */
    l = NULL;
    rc = ledger89_open(&l, missing, LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_ENOENT);
    CHECK(l == NULL);
    rc = ledger89_open(&l, "build/ledger89-no-such-dir", LEDGER89_OPEN_RDWR);
    CHECK_EQ(rc, LEDGER89_ENOENT);
    ledger89_close(NULL);
}

static void test_readonly(void)
{
    fx f;
    unsigned char buf[4];
    size_t size;
    ledger89_slice s;
    ledger89_index idx;
    ledger89_index actual;
    int rc;

    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    s.data = "abc";
    s.size = 3u;
    rc = ledger89_appendv(f.l, &s, 1u, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_OK);
    fx_close(&f);

    f.l = NULL;
    rc = ledger89_open(&f.l, f.path, LEDGER89_OPEN_RDONLY);
    CHECK_EQ(rc, LEDGER89_OK);
    if (rc != LEDGER89_OK)
    {
        return;
    }
    rc = ledger89_read(f.l, test_u64(1), buf, sizeof buf, &size);
    CHECK_EQ(rc, LEDGER89_OK);
    CHECK_EQ(size, 3u);
    rc = ledger89_appendv(f.l, &s, 1u, &idx);
    CHECK_EQ(rc, LEDGER89_EROFS);
    rc = ledger89_sync(f.l, NULL);
    CHECK_EQ(rc, LEDGER89_EROFS);
    rc = ledger89_truncate_from(f.l, test_u64(1));
    CHECK_EQ(rc, LEDGER89_EROFS);
    rc = ledger89_prune_before(f.l, test_u64(1), &actual);
    CHECK_EQ(rc, LEDGER89_EROFS);
    rc = ledger89_rotate(f.l);
    CHECK_EQ(rc, LEDGER89_EROFS);
    ledger89_close(f.l);
}

int main(void)
{
    test_create_and_reopen();
    test_open_errors();
    test_readonly();
    TEST_END;
}
