/* test_conflict_hints.c - AppendEntries conflict acceleration. Covers
 * scenarios in .agent/testing/0014-conflict-hints.md. */
#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_inspect.h"
#include "raft89_internal.h"

static void seed_terms(fixture *f, const unsigned long *terms,
                       unsigned long count)
{
    unsigned long i;

    for (i = 0ul; i < count; ++i)
    {
        CHECK_EQ(fake_store_put(&f->store, terms[i], i + 1ul, NULL, 0u),
                 RAFT89_OK);
    }
}

static void make_follower(fixture *f, raft89 **node)
{
    fake_store_set_hard(&f->store, 1u, 0u);
    *node = NULL;
    CHECK_EQ(raft89_create(&f->config, node), RAFT89_OK);
    CHECK(*node != NULL);
}

static void expect_reply(raft89 *node, int success, unsigned long match,
                         unsigned long conflict_term,
                         unsigned long conflict_index)
{
    raft89_message msg;
    int rc;

    msg.type = RAFT89_MSG_REQUEST_VOTE;
    rc = driver_take_send(node, &msg);
    CHECK_EQ(rc, 1);
    if (rc != 1)
    {
        return;
    }
    CHECK_EQ(msg.type, RAFT89_MSG_APPEND_ENTRIES_RESPONSE);
    CHECK_EQ(msg.u.append_entries_response.success, success);
    CHECK_U64(msg.u.append_entries_response.match_index, test_u64(match));
    CHECK_U64(msg.u.append_entries_response.conflict_term,
              test_u64(conflict_term));
    CHECK_U64(msg.u.append_entries_response.conflict_index,
              test_u64(conflict_index));
}

static void test_follower_hints(void)
{
    static const unsigned long run_terms[7] = {1ul, 1ul, 2ul, 2ul,
                                               2ul, 4ul, 4ul};
    static const unsigned long flat_terms[8] = {7ul, 7ul, 7ul, 7ul,
                                                7ul, 7ul, 7ul, 7ul};
    fixture f;
    raft89 *node;
    raft89_message msg;

    /* Log too short: conflict index is the follower last index + 1. */
    fixture_init(&f, 3u, 1u);
    make_follower(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae(2u, 1u, 1u, 10ul, 1u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_reply(node, 0, 0u, 0u, 1u);
    raft89_destroy(node);

    /* Term mismatch reports the conflicting term's first index. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, run_terms, 7ul);
    make_follower(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae(2u, 1u, 1u, 5ul, 3u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_reply(node, 0, 0u, 2u, 3u);
    raft89_destroy(node);

    /* A conflict run starting at index 1 reports index 1. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, flat_terms, 8ul);
    make_follower(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae(2u, 1u, 1u, 8ul, 9u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_reply(node, 0, 0u, 7u, 1u);
    raft89_destroy(node);

    /* A stale-term rejection still carries a valid hint pair. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, run_terms, 7ul);
    fake_store_set_hard(&f.store, 5u, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae(2u, 1u, 4u, 0ul, 0u, 0u, NULL, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    expect_reply(node, 0, 0u, 0u, 8u);
    raft89_destroy(node);
}

static void make_leader(fixture *f, raft89 **node)
{
    raft89_message msg;

    fake_random_push(&f->random, 0u);
    *node = NULL;
    CHECK_EQ(raft89_create(&f->config, node), RAFT89_OK);
    CHECK(*node != NULL);
    if (*node == NULL)
    {
        return;
    }
    CHECK_EQ(raft89_tick(*node, 20u), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(*node, 1u, 1u), 1);
    driver_drain(*node);
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(*node, &msg), RAFT89_OK);
    driver_drain(*node);
}

static unsigned long peer_next(raft89 *node, unsigned long peer_index)
{
    raft89_id id;
    raft89_index next;
    raft89_index match;
    int rc;

    id = 0u;
    next = raft89_u64_zero();
    match = raft89_u64_zero();
    rc = raft89_inspect_peer(node, peer_index, &id, &next, &match);
    CHECK_EQ(rc, 0);
    return (unsigned long)next.lo;
}

static void test_leader_jumps(void)
{
    static const unsigned long terms[13] = {1ul, 1ul, 2ul, 2ul, 2ul, 2ul, 4ul,
                                            4ul, 4ul, 4ul, 4ul, 4ul, 4ul};
    fixture f;
    raft89 *node;
    raft89_message msg;

    /* Leader contains the conflict term: next_index jumps to its end. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, terms, 13ul);
    make_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    CHECK_EQ(peer_next(node, 0u), 14ul);
    driver_build_ae_response(2u, 1u, 1u, 0, 0u, 2u, 20u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(peer_next(node, 0u), 7ul);
    driver_drain(node);
    raft89_destroy(node);

    /* Leader lacks the conflict term: next_index is the hint index. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, terms, 13ul);
    make_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae_response(2u, 1u, 1u, 0, 0u, 9u, 5u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(peer_next(node, 0u), 5ul);
    driver_drain(node);
    raft89_destroy(node);

    /* A hint past the log end is clamped to last_log_index + 1. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, terms, 13ul);
    make_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae_response(2u, 1u, 1u, 0, 0u, 0u, 51u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    CHECK_EQ(peer_next(node, 0u), 14ul);
    driver_drain(node);
    raft89_destroy(node);

    /* A higher-term rejection steps the leader down before hints apply. */
    fixture_init(&f, 3u, 1u);
    seed_terms(&f, terms, 13ul);
    make_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae_response(2u, 1u, 9u, 0, 0u, 2u, 3u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_OK);
    {
        raft89_status status;
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_EQ(status.role, RAFT89_FOLLOWER);
    }
    driver_drain(node);
    raft89_destroy(node);
}

static void test_malformed_hints(void)
{
    static const unsigned long terms[3] = {1ul, 1ul, 1ul};
    fixture f;
    raft89 *node;
    raft89_message msg;

    fixture_init(&f, 3u, 1u);
    seed_terms(&f, terms, 3ul);
    make_leader(&f, &node);
    if (node == NULL)
    {
        return;
    }
    driver_build_ae_response(2u, 1u, 1u, 1, 0u, 1u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    driver_build_ae_response(2u, 1u, 1u, 1, 0u, 0u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    driver_build_ae_response(2u, 1u, 1u, 0, 0u, 0u, 0u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    driver_build_ae_response(2u, 1u, 1u, 0, 3u, 0u, 1u, &msg);
    CHECK_EQ(raft89_recv(node, &msg), RAFT89_ERR_PROTOCOL);
    CHECK_EQ(peer_next(node, 0u), 4ul);
    driver_drain(node);
    raft89_destroy(node);
}

static void test_64bit_hint(void)
{
    fixture f;
    raft89 *node;
    raft89_message msg;
    raft89__u64 high;
    raft89_u64 want;
    raft89_size slot;

    /* Private setup: a leader whose next_index and last index are above
     * 2^32. The conflict index must survive without truncation. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    if (node == NULL)
    {
        return;
    }
    slot = raft89__member_index(node, 2u);
    high = ((raft89__u64)1 << 32) + (raft89__u64)5;
    want = raft89__to_public(high);
    node->role = RAFT89_LEADER;
    node->current_term = raft89__from_public(test_u64(2u));
    node->last_log_index = high + (raft89__u64)10;
    node->next_index[slot] = high + (raft89__u64)1;
    driver_build_ae_response(2u, 1u, 2u, 0, 0u, 0u, 1u, &msg);
    msg.u.append_entries_response.conflict_index = raft89__to_public(high);
    /* The leader lacks the term; next_index becomes the hint index, and
     * the following send faults on the unreadable artificial index. */
    CHECK(raft89_recv(node, &msg) != RAFT89_OK);
    CHECK(raft89_u64_equal(raft89__to_public(node->next_index[slot]), want));
    raft89_destroy(node);
}

int main(void)
{
    test_follower_hints();
    test_leader_jumps();
    test_malformed_hints();
    test_64bit_hint();
    TEST_END;
}
