/* test_observer.c - OB01..OB03: the advisory observer is called once per
 * record of a successful append and never on rejection. */

#include <string.h>

#include "test.h"

#include "fixture.h"

#define OBS_MAX 8

static ledger89_index obs_index[OBS_MAX];
static unsigned long obs_tag[OBS_MAX];
static size_t obs_count;

static void observer(void *ctx, const ledger89_view *view)
{
    int *marker;

    marker = (int *)ctx;
    ++*marker;
    if (obs_count < OBS_MAX)
    {
        obs_index[obs_count] = view->index;
        obs_tag[obs_count] = view->tag;
    }
    ++obs_count;
}

int main(void)
{
    fx f;
    ledger89_record batch[3];
    int marker;
    unsigned char data[2];

    data[0] = 'a';
    data[1] = 'b';
    CHECK_EQ(fx_open(&f), LEDGER89_OK);

    /* OB01: observer sees each record once in ascending order. */
    obs_count = 0u;
    marker = 0;
    CHECK_EQ(ledger89_set_observer(f.l, observer, &marker), LEDGER89_OK);
    fx_record(&batch[0], 1ul, 100ul, data, sizeof data);
    fx_record(&batch[1], 2ul, 200ul, data, sizeof data);
    fx_record(&batch[2], 3ul, 300ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 3u), LEDGER89_OK);
    CHECK_EQ(obs_count, 3u);
    CHECK_EQ(marker, 3);
    CHECK_EQ(obs_index[0], 1ul);
    CHECK_EQ(obs_index[1], 2ul);
    CHECK_EQ(obs_index[2], 3ul);
    CHECK_EQ(obs_tag[0], 100ul);
    CHECK_EQ(obs_tag[2], 300ul);

    /* OB03: a rejected append never calls the observer. */
    obs_count = 0u;
    fx_record(&batch[0], 9ul, 1ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 1u), LEDGER89_ERR_SEQUENCE);
    CHECK_EQ(obs_count, 0u);

    /* OB02: clearing the observer stops callbacks. */
    obs_count = 0u;
    CHECK_EQ(ledger89_set_observer(f.l, NULL, NULL), LEDGER89_OK);
    fx_record(&batch[0], 4ul, 400ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 1u), LEDGER89_OK);
    CHECK_EQ(obs_count, 0u);

    /* A reopened handle has no observer installed. */
    obs_count = 0u;
    CHECK_EQ(fx_reopen(&f), LEDGER89_OK);
    fx_record(&batch[0], 5ul, 500ul, data, sizeof data);
    CHECK_EQ(ledger89_append(f.l, batch, 1u), LEDGER89_OK);
    CHECK_EQ(obs_count, 0u);

    /* Null handle rejected. */
    CHECK_EQ(ledger89_set_observer(NULL, observer, &marker), LEDGER89_ERR_ARG);

    fx_close(&f);

    TEST_END;
}
