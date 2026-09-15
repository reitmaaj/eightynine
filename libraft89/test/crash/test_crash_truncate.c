/* test_crash_truncate.c - log truncate crash points. Covers CR05. */
#include "test.h"

#include "crash_oracle.h"
#include "driver.h"
#include "fake_app.h"
#include "fixture.h"

static void seed_log(fixture *f)
{
    CHECK_EQ(fake_store_put(&f->store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f->store, 1u, 2u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f->store, 2u, 3u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f->store, 2u, 4u, "b", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f->store, 3u, 5u, "c", 1u), RAFT89_OK);
}

static void truncate_case(int mode, unsigned long expected_count,
                          unsigned long expected_last)
{
    fixture f;
    fake_app app;
    crash_oracle o;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_entry entries[1];

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    seed_log(&f);
    fake_app_init(&app);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    oracle_init(&o, node);
    entries[0].index = test_u64(3u);
    entries[0].term = test_u64(3u);
    entries[0].data = "d";
    entries[0].size = 1u;
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 0u, entries, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_LOG_TRUNCATE);
    CHECK_U64(o.action->u.log_truncate.first_index, test_u64(3u));

    if (mode == 0)
    {
        oracle_crash(&o);
    }
    if (mode == 1)
    {
        CHECK_EQ(oracle_effect_truncate_partial(&o, &f.store, 4u), 0);
        oracle_crash(&o);
    }
    if (mode == 2)
    {
        CHECK_EQ(oracle_effect_truncate_partial(&o, &f.store, 2u), 0);
        oracle_crash(&o);
    }
    if (mode == 3)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
        oracle_crash(&o);
    }

    CHECK_EQ(f.store.entry_count, expected_count);
    CHECK_EQ(oracle_restart(&o, &f.config), RAFT89_OK);
    CHECK_EQ(raft89_status_get(o.raft, &status), RAFT89_OK);
    CHECK_U64(status.last_log_index, test_u64(expected_last));
    raft89_destroy(o.raft);
}

int main(void)
{
    truncate_case(0, 5u, 5u);
    truncate_case(1, 4u, 4u);
    truncate_case(2, 2u, 2u);
    truncate_case(3, 2u, 2u);
    TEST_END;
}
