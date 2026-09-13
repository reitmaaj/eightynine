/* test_open.c - O01..O08: create, reopen, close, busy, null arguments. */

#include <string.h>

#include "test.h"

#include "fixture.h"

int main(void)
{
    fx f;
    ledger89_config config;
    ledger89 *b;
    int i;

    /* O01/O02: a missing path and an empty directory both yield an empty
     * ledger with first_index == 1 and last_index == 0. */
    CHECK_EQ(fx_open(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_first_index(f.l), 1ul);
    CHECK_EQ(ledger89_last_index(f.l), 0ul);

    /* O07: a second simultaneous open is BUSY. */
    b = NULL;
    CHECK_EQ(ledger89_open(&b, &f.config), LEDGER89_ERR_BUSY);
    CHECK(b == NULL);

    /* O04: closing a populated/empty handle leaves it usable on reopen. */
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    CHECK_EQ(ledger89_last_index(f.l), 0ul);

    /* O06: repeated open/close preserves state. */
    for (i = 0; i < 100; ++i)
    {
        CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
        CHECK_EQ(ledger89_first_index(f.l), 1ul);
        CHECK_EQ(ledger89_last_index(f.l), 0ul);
    }
    fx_close(&f);

    /* O08: null arguments. */
    CHECK_EQ(ledger89_open(NULL, &f.config), LEDGER89_ERR_ARG);
    CHECK_EQ(ledger89_open(&b, NULL), LEDGER89_ERR_ARG);
    CHECK(b == NULL);
    memset(&config, 0, sizeof config);
    CHECK_EQ(ledger89_open(&b, &config), LEDGER89_ERR_ARG);
    CHECK(b == NULL);
    config.path = "";
    CHECK_EQ(ledger89_open(&b, &config), LEDGER89_ERR_ARG);

    /* O04: closing NULL is a no-op. */
    ledger89_close(NULL);
    ledger89_iter_close(NULL);

    TEST_END;
}
