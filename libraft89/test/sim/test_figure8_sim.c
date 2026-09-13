/* test_figure8_sim.c - deterministic Figure-8 simulation. See
 * .agent/design/0004-figure8.md. */
#include <string.h>

#include "test.h"

#include "raft_cluster.h"

#define N 5

static cluster c;

static raft89_status status_of(raft89_id id)
{
    raft89_status status;
    CHECK_EQ(raft89_status_get(c.nodes[id - 1u].raft, &status), RAFT89_OK);
    return status;
}

static void partition_pair(raft89_id a, raft89_id b)
{
    cluster_link(&c, a, b, 0);
    cluster_link(&c, b, a, 0);
}

static void heal_all(void)
{
    raft89_id a;
    raft89_id b;
    for (a = 1u; a <= (raft89_id)N; ++a)
    {
        for (b = 1u; b <= (raft89_id)N; ++b)
        {
            cluster_link(&c, a, b, 1);
        }
    }
}

static void expect_applied(raft89_id id, raft89_index index, const char *text)
{
    const unsigned char *data;
    data = cluster_applied(&c, id, index);
    if (data == NULL)
    {
        CHECK(data != NULL);
        return;
    }
    CHECK(strcmp((const char *)data, text) == 0);
}

static void expect_no_x(void)
{
    unsigned long i;
    const unsigned char *data;
    for (i = 1u; i <= N; ++i)
    {
        data = cluster_applied(&c, (raft89_id)i, 2u);
        if (data != NULL)
        {
            CHECK(strcmp((const char *)data, "X") != 0);
        }
    }
}

int main(void)
{
    unsigned long i;

    cluster_init(&c, N, 0u);

    /* Elect S1 in term 1. */
    CHECK_EQ(cluster_tick(&c, 1u, 20u), RAFT89_OK);
    CHECK_EQ(cluster_drain(&c, 1u), 5);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(status_of(1u).role, RAFT89_LEADER);

    /* Commit A at index 1 across the cluster. */
    CHECK_EQ(cluster_propose(&c, 1u, "A", 1u), RAFT89_OK);
    cluster_drain(&c, 1u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(cluster_tick(&c, 1u, 10u), RAFT89_OK);
    cluster_drain(&c, 1u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    for (i = 1u; i <= N; ++i)
    {
        expect_applied((raft89_id)i, 1u, "A");
    }

    /* Partition {1,2} from {3,4,5}; X reaches only S2. */
    partition_pair(1u, 3u);
    partition_pair(1u, 4u);
    partition_pair(1u, 5u);
    partition_pair(2u, 3u);
    partition_pair(2u, 4u);
    partition_pair(2u, 5u);
    CHECK_EQ(cluster_propose(&c, 1u, "X", 1u), RAFT89_OK);
    cluster_drain(&c, 1u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(status_of(1u).commit_index, 1u);
    expect_no_x();

    /* S1 crashes; S5 leads and accepts Y but crashes before replicating. */
    cluster_crash(&c, 1u, 0);
    heal_all();
    CHECK_EQ(cluster_tick(&c, 5u, 20u), RAFT89_OK);
    cluster_drain(&c, 5u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(status_of(5u).role, RAFT89_LEADER);
    CHECK_EQ(cluster_propose(&c, 5u, "Y", 1u), RAFT89_OK);
    cluster_drain(&c, 5u);
    cluster_clear_queue(&c);
    cluster_crash(&c, 5u, 0);

    /* S1 restarts; the first election is blocked by term-2 votes, the
     * second wins in term 3, replicating old-term X to a majority. */
    cluster_restart(&c, 1u);
    CHECK_EQ(cluster_tick(&c, 1u, 20u), RAFT89_OK);
    cluster_drain(&c, 1u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(cluster_tick(&c, 1u, 20u), RAFT89_OK);
    cluster_drain(&c, 1u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(status_of(1u).role, RAFT89_LEADER);
    CHECK(status_of(1u).commit_index < 2u);
    expect_no_x();

    /* S1 crashes; S5 returns in term 4, overwrites X with Y, and commits
     * a current-term entry, applying Y at index 2. */
    cluster_crash(&c, 1u, 0);
    cluster_restart(&c, 5u);
    CHECK_EQ(cluster_tick(&c, 5u, 20u), RAFT89_OK);
    cluster_drain(&c, 5u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(cluster_tick(&c, 5u, 20u), RAFT89_OK);
    cluster_drain(&c, 5u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    CHECK_EQ(status_of(5u).role, RAFT89_LEADER);
    CHECK_EQ(cluster_propose(&c, 5u, "W", 1u), RAFT89_OK);
    cluster_drain(&c, 5u);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    cluster_deliver_all(&c);
    expect_no_x();
    expect_applied(5u, 2u, "Y");

    cluster_free(&c);
    TEST_END;
}
