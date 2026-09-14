/* eintr_main.c - EINTR retry suite for the POSIX backend.
 *
 * Linked with -Wl,--wrap=pread,pwrite,fsync,fdatasync,ftruncate,rename,
 * unlink. Every injected EINTR must be retried transparently: the operation
 * succeeds and the handle is never poisoned. */

#include <string.h>

#include "test.h"

#include "eintr_shim.h"
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

static void case_append(void)
{
    char path[64];
    ledger89 *l;
    ledger89_slice s;
    ledger89_state st;
    unsigned char v;

    v = 'z';
    s.data = &v;
    s.size = 1u;
    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 3ul, 0) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    eintr_arm(EINTR_PWRITE, 0);
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    ledger89_close(l);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(5));
    ledger89_close(l);
}

static void case_sync(void)
{
    char path[64];
    ledger89 *l;
    ledger89_slice s;
    unsigned char v;

    v = 'z';
    s.data = &v;
    s.size = 1u;
    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 3ul, 0) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    eintr_arm(EINTR_FDATASYNC, 0);
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
}

static void case_rotate(int op)
{
    char path[64];
    ledger89 *l;
    ledger89_state st;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 3ul, 0) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    eintr_arm(op, 0);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(4));
    ledger89_close(l);
}

static void case_truncate(int op)
{
    char path[64];
    ledger89 *l;
    ledger89_state st;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 6ul, 1) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    eintr_arm(op, 0);
    CHECK_EQ(ledger89_truncate_from(l, test_u64(3)), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(3));
    ledger89_close(l);
}

static void case_prune(void)
{
    char path[64];
    ledger89 *l;
    ledger89_index actual;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 6ul, 1) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    eintr_arm(EINTR_UNLINK, 0);
    CHECK_EQ(ledger89_prune_before(l, test_u64(4), &actual), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    CHECK_U64(actual, test_u64(4));
    ledger89_close(l);
}

static void case_open(void)
{
    char path[64];
    ledger89 *l;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 4ul, 0) != 0);
    eintr_arm(EINTR_PREAD, 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
}

static void case_read(void)
{
    char path[64];
    ledger89 *l;
    unsigned char buf[2];
    size_t size;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 4ul, 0) != 0);
    CHECK_EQ(open_ledger(path, &l), LEDGER89_OK);
    eintr_arm(EINTR_PREAD, 0);
    CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
}

int main(void)
{
    case_append();
    case_sync();
    case_rotate(EINTR_FDATASYNC);
    case_rotate(EINTR_FSYNC);
    case_rotate(EINTR_RENAME);
    case_truncate(EINTR_UNLINK);
    case_truncate(EINTR_PWRITE);
    case_prune();
    case_open();
    case_read();

    TEST_END;
}
