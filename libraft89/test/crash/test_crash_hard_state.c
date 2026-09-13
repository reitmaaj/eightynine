/* test_crash_hard_state.c - hard-state crash points. Covers CR01, CR02,
 * and CR12. */
#include "test.h"

#include "crash_oracle.h"
#include "driver.h"
#include "fake_app.h"
#include "fixture.h"

static void vote_case(int mode)
{
    fixture f;
    fake_app app;
    crash_oracle o;
    raft89 *node;
    raft89_status status;
    raft89_message msg;

    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 6u, 0u);
    fake_app_init(&app);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    oracle_init(&o, node);
    driver_build_vote_request(3u, 1u, 7u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_HARD_STATE);
    CHECK_EQ(o.action->u.hard_state.state.current_term, 7u);
    CHECK_EQ(o.action->u.hard_state.state.voted_for, 3u);

    if (mode == 0)
    {
        oracle_crash(&o);
    }
    if (mode == 1)
    {
        CHECK_EQ(oracle_effect_hard_partial(&o, &f.store, 0), 0);
        oracle_crash(&o);
    }
    if (mode == 2)
    {
        CHECK_EQ(oracle_effect_hard_partial(&o, &f.store, 1), 0);
        oracle_crash(&o);
    }
    if (mode == 3)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
        oracle_crash(&o);
    }
    if (mode == 4)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
        CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);
        oracle_crash(&o);
    }

    CHECK_EQ(oracle_restart(&o, &f.config), RAFT89_OK);
    CHECK_EQ(raft89_status_get(o.raft, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_EQ(status.leader_id, RAFT89_ID_NONE);
    CHECK_EQ(status.commit_index, 0u);
    CHECK_EQ(status.applied_index, 0u);
    if (mode <= 1)
    {
        CHECK_EQ(status.current_term, 6u);
        CHECK_EQ(status.voted_for, RAFT89_ID_NONE);
    }
    else
    {
        CHECK_EQ(status.current_term, 7u);
        CHECK_EQ(status.voted_for, 3u);
    }
    raft89_destroy(o.raft);
}

int main(void)
{
    vote_case(0);
    vote_case(1);
    vote_case(2);
    vote_case(3);
    vote_case(4);
    TEST_END;
}
