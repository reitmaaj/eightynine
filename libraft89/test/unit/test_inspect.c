/* test_inspect.c - the test-only inspector: view accessors, canonical
 * snapshots, and deep clones that stay behaviorally identical. */
#include <string.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_inspect.h"

#define SNAP_CAP 8192

static unsigned char snap_a[SNAP_CAP];
static unsigned char snap_b[SNAP_CAP];

static unsigned long snapshot_of(const raft89 *node, unsigned char *buf)
{
    unsigned long len;
    len = 0u;
    CHECK_EQ(raft89_inspect_snapshot(node, buf, SNAP_CAP, &len), RAFT89_OK);
    return len;
}

static void expect_same(const raft89 *a, const raft89 *b)
{
    unsigned long la;
    unsigned long lb;
    la = snapshot_of(a, snap_a);
    lb = snapshot_of(b, snap_b);
    CHECK_EQ(la, lb);
    CHECK(memcmp(snap_a, snap_b, (size_t)la) == 0);
}

static void apply_effect(fake_store *store, const raft89_action *action)
{
    unsigned long i;
    if (action->type == RAFT89_ACT_HARD_STATE)
    {
        store->hard = action->u.hard_state.state;
    }
    if (action->type == RAFT89_ACT_LOG_APPEND)
    {
        for (i = 0u; i < action->u.log_append.entry_count; ++i)
        {
            fake_store_put(store, action->u.log_append.entries[i].term,
                           action->u.log_append.entries[i].index,
                           action->u.log_append.entries[i].data,
                           action->u.log_append.entries[i].size);
        }
    }
    if (action->type == RAFT89_ACT_LOG_TRUNCATE)
    {
        fake_store_truncate(store, action->u.log_truncate.first_index);
    }
}

static void step_pair(raft89 *a, fake_store *sa, raft89 *b, fake_store *sb)
{
    const raft89_action *aa;
    const raft89_action *ab;
    aa = NULL;
    ab = NULL;
    CHECK_EQ(raft89_next_action(a, &aa), RAFT89_OK);
    CHECK_EQ(raft89_next_action(b, &ab), RAFT89_OK);
    CHECK_EQ((unsigned long)aa->type, (unsigned long)ab->type);
    apply_effect(sa, aa);
    apply_effect(sb, ab);
    CHECK_EQ(raft89_action_done(a, aa->id, RAFT89_ACTION_OK), RAFT89_OK);
    CHECK_EQ(raft89_action_done(b, ab->id, RAFT89_ACTION_OK), RAFT89_OK);
}

int main(void)
{
    fixture f1;
    fixture f2;
    raft89 *a;
    raft89 *b;
    raft89_inspect_view view;
    raft89_message msg;
    raft89_id pid;
    raft89_index pnext;
    raft89_index pmatch;
    unsigned long len;
    int granted;

    a = NULL;
    b = NULL;
    pid = 0u;
    pnext = 0u;
    pmatch = 0u;
    len = 0u;
    granted = 0;

    fixture_init(&f1, 3u, 1u);
    fake_store_set_hard(&f1.store, 0u, 0u);
    fake_random_push(&f1.random, 0u);
    CHECK_EQ(raft89_create(&f1.config, &a), RAFT89_OK);
    CHECK_EQ(raft89_inspect_clone(a, &b), RAFT89_OK);

    fixture_init(&f2, 3u, 1u);
    f2.store = f1.store;
    f2.random = f1.random;
    fake_store_bind(&f2.config.store, &f2.store);
    fake_random_bind(&f2.config.random, &f2.random);
    raft89_inspect_rebind(b, &f2.config.store, &f2.config.random);

    raft89_inspect_view_get(a, &view);
    CHECK_EQ(view.member_count, 3u);
    CHECK_EQ(view.peer_count, 2u);
    CHECK_EQ(view.role, RAFT89_FOLLOWER);
    CHECK_EQ(view.has_action, 0);
    CHECK_EQ(raft89_inspect_action(a), NULL);
    CHECK_EQ(raft89_inspect_peer(a, 0u, &pid, &pnext, &pmatch), 0);
    CHECK_EQ(pid, 2u);
    CHECK_EQ(raft89_inspect_peer(a, 1u, &pid, &pnext, &pmatch), 0);
    CHECK_EQ(pid, 3u);
    CHECK_EQ(raft89_inspect_peer(a, 2u, &pid, &pnext, &pmatch), -1);
    CHECK_EQ(raft89_inspect_snapshot(a, snap_a, 1u, &len), RAFT89_ERR_LIMIT);
    expect_same(a, b);

    /* Tick into an election, persist hard state, request votes. */
    CHECK_EQ(raft89_tick(a, 20u), RAFT89_OK);
    CHECK_EQ(raft89_tick(b, 20u), RAFT89_OK);
    expect_same(a, b);
    raft89_inspect_view_get(a, &view);
    CHECK_EQ(view.role, RAFT89_CANDIDATE);
    CHECK_EQ(raft89_inspect_vote(a, 0u, &granted), 0);
    CHECK_EQ(granted, 1);
    step_pair(a, &f1.store, b, &f2.store);
    expect_same(a, b);
    step_pair(a, &f1.store, b, &f2.store);
    step_pair(a, &f1.store, b, &f2.store);
    expect_same(a, b);
    CHECK_EQ(raft89_inspect_action_seq(a), 3u);

    /* A granted vote from peer 2 makes both nodes leader in term 1. */
    driver_build_vote_response(2u, 1u, 1u, 1, &msg);
    CHECK_EQ(raft89_recv(a, &msg), RAFT89_OK);
    CHECK_EQ(raft89_recv(b, &msg), RAFT89_OK);
    expect_same(a, b);
    raft89_inspect_view_get(a, &view);
    CHECK_EQ(view.role, RAFT89_LEADER);
    CHECK_EQ(view.leader_id, 1u);
    CHECK_EQ(raft89_inspect_vote(a, 1u, &granted), 0);
    CHECK_EQ(granted, 1);
    CHECK_EQ(raft89_inspect_peer(a, 0u, &pid, &pnext, &pmatch), 0);
    CHECK_EQ(pid, 2u);
    CHECK_EQ(pnext, 1u);
    CHECK_EQ(pmatch, 0u);

    step_pair(a, &f1.store, b, &f2.store);
    step_pair(a, &f1.store, b, &f2.store);
    expect_same(a, b);

    raft89_destroy(a);
    raft89_destroy(b);
    TEST_END;
}
