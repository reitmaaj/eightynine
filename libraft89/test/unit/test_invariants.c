/* test_invariants.c - the invariant engine must accept consistent
 * cluster states and flag each implemented violation. */
#include <string.h>

#include "test.h"

#include "invariants.h"
#include "raft89_internal.h"

static cluster c;
static cluster p;

static void reset(void)
{
    cluster_free(&p);
    cluster_free(&c);
    memset(&p, 0, sizeof(p));
    memset(&c, 0, sizeof(c));
    cluster_init(&c, 3u, 0u);
    cluster_init(&p, 3u, 0u);
}

static void expect_clean(void)
{
    const char *v;
    v = invariants_check(&c);
    if (v != NULL)
    {
        fprintf(stderr, "unexpected violation: %s\n", v);
        ++test_failures;
    }
}

static void expect_violation(const char *tag)
{
    const char *v;
    v = invariants_check(&c);
    if (v == NULL)
    {
        fprintf(stderr, "expected %s violation, got none\n", tag);
        ++test_failures;
        return;
    }
    if (strstr(v, tag) == NULL)
    {
        fprintf(stderr, "expected %s violation, got: %s\n", tag, v);
        ++test_failures;
    }
}

static void expect_transition_clean(void)
{
    const char *v;
    v = invariants_check_transition(&p, &c);
    if (v != NULL)
    {
        fprintf(stderr, "unexpected transition violation: %s\n", v);
        ++test_failures;
    }
}

static void expect_transition_violation(const char *tag)
{
    const char *v;
    v = invariants_check_transition(&p, &c);
    if (v == NULL)
    {
        fprintf(stderr, "expected transition %s violation, got none\n", tag);
        ++test_failures;
        return;
    }
    if (strstr(v, tag) == NULL)
    {
        fprintf(stderr, "expected transition %s violation, got: %s\n", tag, v);
        ++test_failures;
    }
}

static void case_clean_election(void)
{
    unsigned long i;
    reset();
    expect_clean();
    expect_transition_clean();
    CHECK_EQ(cluster_tick(&c, 1u, 20u), RAFT89_OK);
    CHECK_EQ(cluster_drain(&c, 1u), 3);
    expect_clean();
    for (i = 0u; i < 4u; ++i)
    {
        cluster_deliver_all(&c);
        expect_clean();
    }
}

static void case_i01(void)
{
    reset();
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 1u;
    c.nodes[1].raft->role = RAFT89_LEADER;
    c.nodes[1].raft->current_term = 1u;
    expect_violation("I01");
}

static void case_i03(void)
{
    reset();
    fake_store_set_hard(&p.nodes[0].f.store, 1u, 2u);
    fake_store_set_hard(&c.nodes[0].f.store, 1u, 3u);
    expect_transition_violation("I03");
    fake_store_set_hard(&c.nodes[0].f.store, 1u, 2u);
    expect_transition_clean();
    /* Different nodes voting differently in one term is legal. */
    fake_store_set_hard(&c.nodes[1].f.store, 1u, 3u);
    expect_transition_clean();
    expect_clean();
}

static void case_i04(void)
{
    reset();
    fake_store_set_hard(&c.nodes[0].f.store, 1u, 99u);
    expect_violation("I04");
}

static void case_i05(void)
{
    reset();
    fake_store_put(&c.nodes[0].f.store, 1u, 1u, "A", 1u);
    fake_store_put(&c.nodes[1].f.store, 1u, 1u, "B", 1u);
    expect_violation("I05");
    fake_store_put(&c.nodes[1].f.store, 1u, 1u, "A", 1u);
    expect_clean();
}

static void case_i08_i09(void)
{
    reset();
    c.nodes[0].app.rec[1].index = 1u;
    c.nodes[0].app.rec[1].term = 1u;
    c.nodes[0].app.rec[1].hash = 111u;
    c.nodes[0].app.rec[1].present = 1;
    c.nodes[0].app.last_applied = 1u;
    c.nodes[1].app.rec[1].index = 1u;
    c.nodes[1].app.rec[1].term = 1u;
    c.nodes[1].app.rec[1].hash = 222u;
    c.nodes[1].app.rec[1].present = 1;
    c.nodes[1].app.last_applied = 1u;
    expect_violation("I08");
    c.nodes[1].app.rec[1].hash = 111u;
    expect_clean();
    c.nodes[0].app.last_applied = 2u;
    expect_violation("I09");
}

static void case_i10_i16(void)
{
    reset();
    c.nodes[0].raft->last_log_index = 0u;
    c.nodes[0].raft->commit_index = 2u;
    expect_violation("I10");
    reset();
    c.nodes[0].raft->last_log_index = 1u;
    c.nodes[0].raft->last_log_term = 1u;
    c.nodes[0].raft->commit_index = 1u;
    c.nodes[0].raft->applied_index = 2u;
    expect_violation("I16");
}

static void case_i14(void)
{
    reset();
    c.nodes[0].raft->current_term = 5u;
    expect_violation("I14");
    reset();
    c.nodes[0].raft->voted_for = 2u;
    expect_violation("I14");
    reset();
    c.nodes[0].raft->last_log_index = 1u;
    expect_violation("I14");
}

static void case_i14_windows(void)
{
    reset();
    /* A durable vote may lead a still-NONE acknowledged vote. */
    c.nodes[0].raft->current_term = 1u;
    fake_store_set_hard(&c.nodes[0].f.store, 1u, 2u);
    expect_clean();
    /* An acknowledged vote the store lacks is a violation. */
    c.nodes[0].raft->voted_for = 3u;
    expect_violation("I14");
    reset();
    /* A pending truncate may leave the durable log shorter. */
    c.nodes[0].raft->last_log_index = 2u;
    c.nodes[0].raft->last_log_term = 1u;
    c.nodes[0].raft->has_action = 1;
    c.nodes[0].raft->action.type = RAFT89_ACT_LOG_TRUNCATE;
    c.nodes[0].raft->action.u.log_truncate.first_index = test_u64(1u);
    c.nodes[0].oracle.phase = ORACLE_EFFECT_DONE;
    expect_clean();
    /* Removing below first_index - 1 breaks the acknowledged prefix. */
    c.nodes[0].raft->action.u.log_truncate.first_index = test_u64(3u);
    expect_violation("I14");
}

static void case_i07(void)
{
    reset();
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 2u;
    c.nodes[1].raft->current_term = 1u;
    c.nodes[1].raft->last_log_index = 1u;
    c.nodes[1].raft->last_log_term = 1u;
    c.nodes[1].raft->commit_index = 1u;
    fake_store_set_hard(&c.nodes[0].f.store, 2u, 0u);
    fake_store_set_hard(&c.nodes[1].f.store, 1u, 0u);
    fake_store_put(&c.nodes[1].f.store, 1u, 1u, "A", 1u);
    expect_violation("I07");
}

static void case_i15_i17(void)
{
    reset();
    c.nodes[0].raft->has_action = 1;
    c.nodes[0].raft->action.type = RAFT89_ACT_SEND;
    c.nodes[0].raft->action.u.send.message.type =
        RAFT89_MSG_REQUEST_VOTE_RESPONSE;
    c.nodes[0].raft->action.u.send.message.u.request_vote_response.term =
        test_u64(1u);
    c.nodes[0]
        .raft->action.u.send.message.u.request_vote_response.vote_granted = 1;
    expect_violation("I15");
    reset();
    c.nodes[0].raft->has_action = 1;
    expect_violation("I17");
}

static void case_transitions(void)
{
    reset();
    expect_transition_clean();
    p.nodes[0].raft->current_term = 1u;
    expect_transition_violation("I02");
    reset();
    p.nodes[0].raft->commit_index = 1u;
    expect_transition_violation("I11");
    reset();
    p.nodes[0].raft->applied_index = 1u;
    expect_transition_violation("I12");
    reset();
    p.nodes[0].raft->role = RAFT89_LEADER;
    p.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 1u;
    fake_store_put(&p.nodes[0].f.store, 1u, 1u, "A", 1u);
    expect_transition_violation("I06");
    reset();
    p.nodes[0].raft->role = RAFT89_LEADER;
    p.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->commit_index = 1u;
    fake_store_put(&c.nodes[0].f.store, 2u, 1u, "A", 1u);
    expect_transition_violation("I13");
    reset();
    p.nodes[0].raft->role = RAFT89_LEADER;
    p.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->commit_index = 1u;
    fake_store_put(&c.nodes[0].f.store, 1u, 1u, "A", 1u);
    expect_transition_violation("I13");
    reset();
    p.nodes[0].raft->role = RAFT89_LEADER;
    p.nodes[0].raft->current_term = 1u;
    c.nodes[0].raft->role = RAFT89_LEADER;
    c.nodes[0].raft->current_term = 1u;
    p.nodes[0].raft->match_index[1] = 1u;
    expect_transition_violation("I19");
}

int main(void)
{
    case_clean_election();
    case_i01();
    case_i03();
    case_i04();
    case_i05();
    case_i08_i09();
    case_i10_i16();
    case_i14();
    case_i14_windows();
    case_i07();
    case_i15_i17();
    case_transitions();
    cluster_free(&c);
    cluster_free(&p);
    TEST_END;
}
