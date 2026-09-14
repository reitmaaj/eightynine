/* test_auto_rotate.c - white-box automatic rotation when the active part
 * exceeds the internal byte target. */

#include <string.h>

#include "ledger89_internal.h"
#include "test.h"
#include "tmpdir.h"

int main(void)
{
    char path[64];
    ledger89 *l;
    ledger89_slice s;
    ledger89_state st;
    unsigned char buf[32];
    size_t size;
    unsigned long i;

    if (tmpdir_create(path, sizeof path) != 0)
    {
        return 1;
    }
    l = NULL;
    CHECK_EQ(led89_open_io(&l, path,
                           LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                               LEDGER89_OPEN_EXCL,
                           led89_io_posix()),
             LEDGER89_OK);
    if (l == NULL)
    {
        return 1;
    }
    l->part_target = 200;
    s.data = "0123456789abcdef";
    s.size = 16u;
    for (i = 0ul; i < 64ul; ++i)
    {
        CHECK_EQ(ledger89_appendv(l, &s, 1u, NULL), LEDGER89_OK);
    }
    CHECK_EQ(ledger89_sync(l, NULL), LEDGER89_OK);
    CHECK(l->part_count >= 2u);
    ledger89_close(l);

    l = NULL;
    CHECK_EQ(ledger89_open(&l, path, LEDGER89_OPEN_RDWR), LEDGER89_OK);
    if (l == NULL)
    {
        return 1;
    }
    CHECK_EQ(ledger89_get_state(l, &st), LEDGER89_OK);
    CHECK_U64(st.end, test_u64(65));
    CHECK_EQ(ledger89_read(l, test_u64(1), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 16u);
    CHECK(memcmp(buf, "0123456789abcdef", 16u) == 0);
    CHECK_EQ(ledger89_read(l, test_u64(64), buf, sizeof buf, &size),
             LEDGER89_OK);
    CHECK_EQ(size, 16u);
    CHECK_EQ(ledger89_verify(l), LEDGER89_OK);
    ledger89_close(l);
    TEST_END;
}
