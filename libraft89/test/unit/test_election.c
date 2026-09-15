/* test_election.c - elections, quorum counting, and leadership
 * acquisition. Covers scenarios E01-E12, E15-E20. */
#include "test.h"

#include "driver.h"
#include "fixture.h"

static void run_election(fixture *f, raft89 *node)
{
    CHECK_EQ(raft89_tick(node, f->config.election_timeout_min), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 1u, f->config.self), 1);
    driver_drain(node);
}

static void check_quorum(raft89_size size, raft89_size grants)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_size i;
    raft89_size quorum;

    fixture_init(&f, size, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    run_election(&f, node);
    for (i = 0u; i < grants; ++i)
    {
        driver_build_vote_response((raft89_id)(i + 2u), 1u, 1u, 1, &msg);
        CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    }
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    quorum = (size / 2u) + 1u;
    if (grants + 1u >= quorum)
    {
        CHECK_EQ(status.role, RAFT89_LEADER);
    }
    else
    {
        CHECK_EQ(status.role, RAFT89_CANDIDATE);
    }
    raft89_destroy(node);
}

int main(void)
{
    fixture f;
    raft89 *node;
    raft89_status status;
    raft89_message msg;
    raft89_id to[8];
    unsigned long n;

    /* E01-E06: election start, self vote, hard state first, broadcasts. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_CANDIDATE);
    CHECK_U64(status.current_term, RAFT89_TERM_NONE);
    CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_U64(status.current_term, test_u64(1u));
    CHECK_EQ(status.voted_for, 1u);
    n = driver_collect_sends(node, to, 8u);
    CHECK_EQ(n, 2u);
    CHECK_EQ(to[0], 2u);
    CHECK_EQ(to[1], 3u);
    raft89_destroy(node);

    /* E07: a quorum of grants makes the candidate leader. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);
    CHECK_EQ(status.leader_id, 1u);
    n = driver_collect_sends(node, to, 8u);
    CHECK_EQ(n, 2u);
    raft89_destroy(node);

    /* E08: below a quorum the candidate remains a candidate. */
    fixture_init(&f, 4u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_CANDIDATE);
    raft89_destroy(node);

    /* E09: duplicate grants count once. */
    fixture_init(&f, 4u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_CANDIDATE);
    driver_build_vote_response(3u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);
    raft89_destroy(node);

    /* E10: refusals are not counted. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    driver_build_vote_response(2u, 1u, 1u, 0, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_CANDIDATE);
    raft89_destroy(node);

    /* E11-E12: a higher-term response persists hard state before any
     * further message. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    driver_build_vote_response(2u, 1u, 2u, 0, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, RAFT89_ID_NONE), 1);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_FOLLOWER);
    CHECK_U64(status.current_term, test_u64(2u));
    raft89_destroy(node);

    /* E15: a candidate that times out starts a new election. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    run_election(&f, node);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    raft89_destroy(node);

    /* E16: a single-node cluster becomes leader after durable self-vote. */
    fixture_init(&f, 1u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
    CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
    CHECK_EQ(status.role, RAFT89_LEADER);
    CHECK_EQ(status.leader_id, 1u);
    n = driver_collect_sends(node, to, 8u);
    CHECK_EQ(n, 0u);
    raft89_destroy(node);

    /* E17-E20: quorum sizes. */
    check_quorum(2u, 0u);
    check_quorum(2u, 1u);
    check_quorum(3u, 0u);
    check_quorum(3u, 1u);
    check_quorum(4u, 1u);
    check_quorum(4u, 2u);
    check_quorum(5u, 1u);
    check_quorum(5u, 2u);

    TEST_END;
}
