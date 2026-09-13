/* smoke.c - end-to-end open/append/sync/close/reopen/read.
 *
 * The first failing test of the project: it encodes the primary behavior
 * path before any implementation exists. */

#include "test.h"

#include "tmpdir.h"

int main(void)
{
    char path[64];
    ledger89_config config;
    ledger89 *l;
    ledger89_record rec;
    ledger89_view view;
    const unsigned char payload[3] = {'a', 'b', 'c'};

    if (tmpdir_create(path, sizeof path) != 0)
    {
        fprintf(stderr, "cannot create temporary directory\n");
        return 1;
    }

    memset(&config, 0, sizeof config);
    config.path = path;
    config.max_segment_bytes = 1024ul;
    config.max_segment_records = 0ul;

    l = NULL;
    CHECK_EQ(ledger89_open(&l, &config), LEDGER89_OK);
    CHECK(l != NULL);
    if (l == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(ledger89_first_index(l), 1ul);
    CHECK_EQ(ledger89_last_index(l), 0ul);

    memset(&rec, 0, sizeof rec);
    rec.index = 1ul;
    rec.tag = 7ul;
    rec.data = payload;
    rec.size = sizeof payload;
    CHECK_EQ(ledger89_append(l, &rec, 1u), LEDGER89_OK);
    CHECK_EQ(ledger89_sync(l), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(l), 1ul);

    ledger89_close(l);

    l = NULL;
    CHECK_EQ(ledger89_open(&l, &config), LEDGER89_OK);
    CHECK(l != NULL);
    if (l == NULL)
    {
        TEST_END;
    }
    CHECK_EQ(ledger89_last_index(l), 1ul);

    memset(&view, 0, sizeof view);
    CHECK_EQ(ledger89_read(l, 1ul, &view), LEDGER89_OK);
    CHECK_EQ(view.index, 1ul);
    CHECK_EQ(view.tag, 7ul);
    CHECK_EQ(view.size, sizeof payload);
    CHECK(memcmp(view.data, payload, sizeof payload) == 0);

    ledger89_close(l);

    TEST_END;
}
