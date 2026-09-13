/* eintr_main.c - EINTR retry suite for the POSIX backend.
 *
 * Linked with -Wl,--wrap=pread,pwrite,fsync,fdatasync,ftruncate,rename,
 * unlink. Every injected EINTR must be retried transparently: the operation
 * succeeds and the handle is never faulted. */

#include <string.h>

#include "test.h"

#include "eintr_shim.h"
#include "tmpdir.h"

static int open_path(const char *path, unsigned long records, ledger89 **l)
{
    ledger89_config cfg;

    memset(&cfg, 0, sizeof cfg);
    cfg.path = path;
    cfg.max_segment_bytes = 0ul;
    cfg.max_segment_records = records;
    *l = NULL;
    return ledger89_open(l, &cfg);
}

static int make_ledger(const char *path, unsigned long records,
                       ledger89_index count)
{
    ledger89 *l;
    ledger89_record r;
    unsigned char v;
    ledger89_index i;

    if (open_path(path, records, &l) != LEDGER89_OK)
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

static void case_append(void)
{
    char path[64];
    ledger89 *l;
    ledger89_record r;
    unsigned char v;

    v = 'z';
    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 0ul, 3ul) != 0);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    r.index = 4ul;
    r.tag = 4ul;
    r.data = &v;
    r.size = 1u;
    eintr_arm(EINTR_PWRITE, 0);
    CHECK_EQ(ledger89_append(l, &r, 1u), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    ledger89_close(l);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 4ul);
    ledger89_close(l);
}

static void case_sync(void)
{
    char path[64];
    ledger89 *l;
    ledger89_record r;
    unsigned char v;

    v = 'z';
    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 0ul, 3ul) != 0);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    r.index = 4ul;
    r.tag = 4ul;
    r.data = &v;
    r.size = 1u;
    CHECK_EQ(ledger89_append(l, &r, 1u), LEDGER89_OK);
    eintr_arm(EINTR_FDATASYNC, 0);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 4ul);
    ledger89_close(l);
}

static void case_rotate(int op)
{
    char path[64];
    ledger89 *l;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 0ul, 3ul) != 0);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    eintr_arm(op, 0);
    CHECK_EQ(ledger89_rotate(l), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_path(path, 0ul, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 3ul);
    ledger89_close(l);
}

static void case_truncate(int op)
{
    char path[64];
    ledger89 *l;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 2ul, 4ul) != 0);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    eintr_arm(op, 0);
    CHECK_EQ(ledger89_truncate_after(l, 1ul), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 1ul);
    ledger89_close(l);
}

static void case_discard(void)
{
    char path[64];
    ledger89 *l;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 2ul, 6ul) != 0);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    eintr_arm(EINTR_UNLINK, 0);
    CHECK_EQ(ledger89_discard_before(l, 4ul), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(l), 4ul);
    ledger89_close(l);
}

static void case_open(void)
{
    char path[64];
    ledger89 *l;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 2ul, 4ul) != 0);
    eintr_arm(EINTR_PREAD, 0);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    CHECK(eintr_fired() != 0);
    ledger89_close(l);
}

static void case_read(void)
{
    char path[64];
    ledger89 *l;
    ledger89_view v;

    CHECK(tmpdir_create(path, sizeof path) == 0);
    CHECK(make_ledger(path, 2ul, 4ul) != 0);
    CHECK_EQ(open_path(path, 2ul, &l), LEDGER89_OK);
    eintr_arm(EINTR_PREAD, 0);
    CHECK_EQ(ledger89_read(l, 1ul, &v), LEDGER89_OK);
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
    case_discard();
    case_open();
    case_read();

    TEST_END;
}
