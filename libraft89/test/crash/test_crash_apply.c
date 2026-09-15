/* test_crash_apply.c - application crash points. Covers CR09, CR10, and
 * CR11. */
#include "test.h"

#include "crash_oracle.h"
#include "driver.h"
#include "fake_app.h"
#include "fixture.h"

static void apply_case(int mode)
{
    fixture f;
    fake_app app;
    crash_oracle o;
    raft89 *node;
    raft89_status status;
    raft89_message msg;

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 1u, 0u);
    CHECK_EQ(fake_store_put(&f.store, 1u, 1u, "a", 1u), RAFT89_OK);
    CHECK_EQ(fake_store_put(&f.store, 1u, 2u, "b", 1u), RAFT89_OK);
    fake_app_init(&app);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    oracle_init(&o, node);
    driver_build_ae(2u, 1u, 1u, 2u, 1u, 2u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_APPLY);
    CHECK_U64(o.action->u.apply.entry.index, test_u64(1u));

    if (mode == 0)
    {
        oracle_crash(&o);
        CHECK_EQ(app.applied_count, 0u);
    }
    if (mode == 1)
    {
        CHECK_EQ(oracle_effect_apply_partial(&o, &app, 1), 0);
        oracle_crash(&o);
    }
    if (mode == 2)
    {
        CHECK_EQ(oracle_effect_apply_partial(&o, &app, 1), 0);
        CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);
        oracle_crash(&o);
    }

    CHECK_EQ(oracle_restart(&o, &f.config), RAFT89_OK);
    CHECK_EQ(raft89_status_get(o.raft, &status), RAFT89_OK);
    CHECK_U64(status.applied_index, test_u64(0u));
    CHECK_U64(status.commit_index, test_u64(0u));

    /* Replay: the same committed entries are offered again. */
    CHECK_EQ(raft89_recv(o.raft, &msg), RAFT89_OK);
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_APPLY);
    CHECK_U64(o.action->u.apply.entry.index, test_u64(1u));
    CHECK_U64(o.action->u.apply.entry.term, test_u64(1u));
    CHECK_EQ(o.action->u.apply.entry.size, 1u);
    CHECK_EQ(oracle_effect_apply_partial(&o, &app, 1), 0);
    CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);
    if (mode == 0)
    {
        CHECK_EQ(app.applied_count, 1u);
        CHECK_EQ(app.replay_count, 0u);
    }
    else
    {
        CHECK_EQ(app.applied_count, 1u);
        CHECK_EQ(app.replay_count, 1u);
    }

    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_APPLY);
    CHECK_U64(o.action->u.apply.entry.index, test_u64(2u));
    CHECK_EQ(oracle_effect_apply_partial(&o, &app, 1), 0);
    CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);
    CHECK_EQ(app.applied_count, 2u);

    /* CR11: a conflicting replay is corruption. */
    {
        raft89_entry bad;
        bad.index = test_u64(1u);
        bad.term = test_u64(9u);
        bad.data = "a";
        bad.size = 1u;
        CHECK_EQ(fake_app_apply(&app, &bad), -1);
    }
    raft89_destroy(o.raft);
}

int main(void)
{
    apply_case(0);
    apply_case(1);
    apply_case(2);
    TEST_END;
}
