/* test_actions.c - action protocol mechanics. Covers scenarios A01-A05,
 * A09, A10, A12-A14, A16-A18. */
#include "test.h"

#include "driver.h"
#include "fixture.h"

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    const raft89_action *first;
    const raft89_action *again;
    raft89_action_id first_id;

    /* A01: no action outstanding on a fresh node. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        TEST_END;
    }
    first = NULL;
    CHECK_EQ(raft89_next_action(node, &first), RAFT89_EMPTY);
    CHECK(first == NULL);

    /* A08: acknowledge with no outstanding action. */
    CHECK_EQ(raft89_action_done(node, 1u, RAFT89_ACTION_OK), RAFT89_ERR_STATE);

    /* A07: acknowledge an arbitrary id with no outstanding action. */
    CHECK_EQ(raft89_action_done(node, 42u, RAFT89_ACTION_OK), RAFT89_ERR_STATE);

    /* Start an election to obtain an action. */
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);

    /* A02/A03: non-zero id, stable across repeated peeks. */
    first = NULL;
    CHECK_EQ(raft89_next_action(node, &first), RAFT89_OK);
    CHECK(first != NULL);
    if (first == NULL)
    {
        TEST_END;
    }
    CHECK(first->id != RAFT89_ACTION_ID_NONE);
    CHECK_EQ(first->type, RAFT89_ACT_HARD_STATE);
    first_id = first->id;
    again = NULL;
    CHECK_EQ(raft89_next_action(node, &again), RAFT89_OK);
    CHECK(again == first);
    CHECK_EQ(again->id, first_id);

    /* A04/A05: events are busy while an action is outstanding. */
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_BUSY);
    driver_build_vote_request(2u, 1u, 1u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_BUSY);

    /* A10: LOST is rejected for a durable action and changes nothing. */
    CHECK_EQ(raft89_action_done(node, first_id, RAFT89_ACTION_LOST),
             RAFT89_ERR_STATE);
    again = NULL;
    CHECK_EQ(raft89_next_action(node, &again), RAFT89_OK);
    CHECK(again == first);

    /* A18: acknowledging produces the next action immediately. */
    CHECK_EQ(raft89_action_done(node, first_id, RAFT89_ACTION_OK), RAFT89_OK);
    again = NULL;
    CHECK_EQ(raft89_next_action(node, &again), RAFT89_OK);
    CHECK(again != NULL);
    if (again != NULL)
    {
        CHECK_EQ(again->type, RAFT89_ACT_SEND);
        CHECK_EQ(again->id, first_id + 1u);

        /* A09: LOST on a SEND is accepted and the sequence continues. */
        CHECK_EQ(raft89_action_done(node, again->id, RAFT89_ACTION_LOST),
                 RAFT89_OK);
        again = NULL;
        CHECK_EQ(raft89_next_action(node, &again), RAFT89_OK);
        CHECK(again != NULL);
        if (again != NULL)
        {
            CHECK_EQ(again->type, RAFT89_ACT_SEND);
            CHECK_EQ(raft89_action_done(node, again->id, RAFT89_ACTION_OK),
                     RAFT89_OK);
        }
    }
    raft89_destroy(node);

    /* A12-A14, A16-A17: FATAL faults the node safely. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    first = NULL;
    CHECK_EQ(raft89_next_action(node, &first), RAFT89_OK);
    CHECK(first != NULL);
    if (first != NULL)
    {
        CHECK_EQ(raft89_action_done(node, first->id, RAFT89_ACTION_FATAL),
                 RAFT89_OK);
    }
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_ERR_FAULTED);
    driver_build_vote_request(2u, 1u, 1u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_FAULTED);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.faulted, 1);
    raft89_destroy(node);

    /* Null arguments. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node != NULL)
    {
        CHECK_EQ(raft89_next_action(NULL, &again), RAFT89_ERR_ARG);
        CHECK_EQ(raft89_next_action(node, NULL), RAFT89_ERR_ARG);
        CHECK_EQ(raft89_action_done(NULL, 1u, RAFT89_ACTION_OK),
                 RAFT89_ERR_ARG);
        CHECK_EQ(raft89_tick(NULL, 1u), RAFT89_ERR_ARG);
        CHECK_EQ(raft89_recv(NULL, &msg), RAFT89_ERR_ARG);
        CHECK_EQ(raft89_recv(node, NULL), RAFT89_ERR_ARG);
        raft89_destroy(node);
    }

    TEST_END;
}
