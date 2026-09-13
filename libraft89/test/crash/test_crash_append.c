/* test_crash_append.c - log append crash points. Covers CR03, CR04, and
 * CR12. */
#include "test.h"

#include "crash_oracle.h"
#include "driver.h"
#include "fake_app.h"
#include "fixture.h"

static void append_case(int mode, unsigned long expected_count,
                        raft89_index expected_last)
{
    fixture f;
    fake_app app;
    crash_oracle o;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_entry entries[2];

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    fake_app_init(&app);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    oracle_init(&o, node);
    entries[0].index = 1u;
    entries[0].term = 1u;
    entries[0].data = "a";
    entries[0].size = 1u;
    entries[1].index = 2u;
    entries[1].term = 1u;
    entries[1].data = "b";
    entries[1].size = 1u;
    driver_build_ae(2u, 1u, 1u, 0u, 0u, 0u, entries, 2u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_LOG_APPEND);

    if (mode == 0)
    {
        oracle_crash(&o);
    }
    if (mode == 1)
    {
        CHECK_EQ(oracle_effect_append_partial(&o, &f.store, 1u), 0);
        oracle_crash(&o);
    }
    if (mode == 2)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
        oracle_crash(&o);
    }
    if (mode == 3)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
        CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);
        oracle_crash(&o);
    }

    CHECK_EQ(f.store.entry_count, expected_count);
    CHECK_EQ(oracle_restart(&o, &f.config), RAFT89_OK);
    CHECK_EQ(raft89_status_get(o.raft, &status), RAFT89_OK);
    CHECK_EQ(status.last_log_index, expected_last);
    CHECK_EQ(status.commit_index, 0u);
    CHECK_EQ(status.applied_index, 0u);
    raft89_destroy(o.raft);
}

int main(void)
{
    append_case(0, 0u, 0u);
    append_case(1, 1u, 1u);
    append_case(2, 2u, 2u);
    append_case(3, 2u, 2u);
    TEST_END;
}
