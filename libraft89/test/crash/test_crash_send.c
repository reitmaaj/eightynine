/* test_crash_send.c - send crash ambiguity. Covers CR07. */
#include "test.h"

#include "crash_oracle.h"
#include "driver.h"
#include "fake_app.h"
#include "fixture.h"

static void send_case(int packet_accepted)
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
    CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
    CHECK_EQ(oracle_ack(&o, RAFT89_ACTION_OK), RAFT89_OK);

    /* The vote response is now an outstanding send. A crash during that
     * send may leave the packet absent or complete; both are accepted. */
    CHECK_EQ(oracle_peek(&o), 1);
    CHECK_EQ(o.action->type, RAFT89_ACT_SEND);
    CHECK_EQ(o.action->u.send.message.type, RAFT89_MSG_REQUEST_VOTE_RESPONSE);
    if (packet_accepted != 0)
    {
        CHECK_EQ(oracle_effect_full(&o, &f.store, &app), 0);
    }
    oracle_crash(&o);

    CHECK_EQ(oracle_restart(&o, &f.config), RAFT89_OK);
    CHECK_EQ(raft89_status_get(o.raft, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_EQ(status.current_term, 7u);
    CHECK_EQ(status.voted_for, 3u);
    raft89_destroy(o.raft);
}

int main(void)
{
    send_case(0);
    send_case(1);
    TEST_END;
}
