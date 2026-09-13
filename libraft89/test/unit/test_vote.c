/* test_vote.c - RequestVote handling and vote persistence. Covers
 * scenarios V01-V12 and V14. */
#include "test.h"

#include "driver.h"
#include "fixture.h"

static raft89 *make_node(fixture *f, raft89_size size, raft89_id self)
{
    raft89 *node;
    fixture_init(f, size, self);
    node = NULL;
    CHECK_EQ(raft89_create(&f->config, &node), RAFT89_OK);
    return node;
}

static void expect_refusal(raft89 *node, raft89_term term)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_SEND);
    CHECK_EQ(action->u.send.message.type, RAFT89_MSG_REQUEST_VOTE_RESPONSE);
    CHECK_EQ(action->u.send.message.u.request_vote_response.term, term);
    CHECK_EQ(action->u.send.message.u.request_vote_response.vote_granted, 0);
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

static void expect_grant(raft89 *node, raft89_term term)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_OK);
    if (action == NULL)
    {
        return;
    }
    CHECK_EQ(action->type, RAFT89_ACT_SEND);
    CHECK_EQ(action->u.send.message.type, RAFT89_MSG_REQUEST_VOTE_RESPONSE);
    CHECK_EQ(action->u.send.message.u.request_vote_response.term, term);
    CHECK_EQ(action->u.send.message.u.request_vote_response.vote_granted, 1);
    CHECK_EQ(raft89_action_done(node, action->id, RAFT89_ACTION_OK), RAFT89_OK);
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;

    /* V01: stale request term is refused with the current term. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 4u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_refusal(node, 5u);
    raft89_destroy(node);

    /* V02: current term, no prior vote, fresh log grants. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 5u, 2u), 1);
    expect_grant(node, 5u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.voted_for, 2u);
    raft89_destroy(node);

    /* V03: a stale candidate log is refused with no persistent change. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    CHECK_EQ(fake_store_append(&f.store, 2u, 1u, NULL, 0u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_refusal(node, 5u);
    raft89_destroy(node);

    /* V04: the same candidate already granted is granted again. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 2u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 5u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_grant(node, 5u);
    raft89_destroy(node);

    /* V05: a different candidate already granted is refused. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 3u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_refusal(node, 5u);
    raft89_destroy(node);

    /* V06/V08: a higher-term fresh request persists term and candidate
     * atomically before the grant is emitted. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 6u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 6u, 2u), 1);
    expect_grant(node, 6u);
    raft89_destroy(node);

    /* V07: a higher-term stale request clears the old vote. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 3u);
    CHECK_EQ(fake_store_append(&f.store, 2u, 1u, NULL, 0u), RAFT89_OK);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 6u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 6u, RAFT89_ID_NONE), 1);
    expect_refusal(node, 6u);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.voted_for, RAFT89_ID_NONE);
    raft89_destroy(node);

    /* V09: a duplicate request stays stable and is granted again. */
    fixture_init(&f, 3u, 1u);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(2u, 1u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 5u, 2u), 1);
    expect_grant(node, 5u);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_grant(node, 5u);
    raft89_destroy(node);

    /* V10: unknown sender is a protocol error with no state change. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    driver_build_vote_request(9u, 1u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    {
        const raft89_action *action;
        action = NULL;
        CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    }

    /* V11: an invalid vote boolean is rejected. */
    driver_build_vote_response(2u, 1u, 1u, 2, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);

    /* V12: a response from an unknown node is rejected. */
    driver_build_vote_response(9u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);

    /* Wrong destination is rejected. */
    driver_build_vote_request(2u, 3u, 5u, 1u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    raft89_destroy(node);

    /* V14: the self vote counts exactly once. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
    driver_drain(node);
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);
    raft89_destroy(node);

    TEST_END;
}
