/* test_timers.c - logical time accounting and timeout selection.
 * Covers scenarios T01-T05, T10, and T11. */
#include <limits.h>

#include "test.h"

#include "driver.h"
#include "fixture.h"
#include "raft89_internal.h"

static void expect_no_action(raft89 *node)
{
    const raft89_action *action;
    action = NULL;
    CHECK_EQ(raft89_next_action(node, &action), RAFT89_EMPTY);
    CHECK(action == NULL);
}

static void expect_election(raft89 *node)
{
    CHECK_EQ(driver_expect_hard_state(node, 1u, 1u), 1);
    driver_drain(node);
}

int main(void)
{
    fixture f;
    raft89 *node;

    /* Pure time saturation. */
    CHECK_EQ(raft89__sat_add(1u, 2u), 3u);
    CHECK_EQ(raft89__sat_add(ULONG_MAX, 1u), ULONG_MAX);
    CHECK_EQ(raft89__sat_add(ULONG_MAX - 5u, 5u), ULONG_MAX);

    /* T01: below the timeout no action. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 19u), RAFT89_OK);
    expect_no_action(node);

    /* T02: reaching the timeout starts an election. */
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_OK);
    expect_election(node);
    raft89_destroy(node);

    /* T03: one transition per tick even far beyond the timeout. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 1000u), RAFT89_OK);
    expect_election(node);
    expect_no_action(node);
    raft89_destroy(node);

    /* T04: random value 0 selects the minimum timeout. */
    fixture_init(&f, 3u, 1u);
    fake_random_push(&f.random, 0u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 19u), RAFT89_OK);
    expect_no_action(node);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_OK);
    expect_election(node);
    raft89_destroy(node);

    /* T05: the range maximum selects the maximum timeout. */
    fixture_init(&f, 3u, 1u);
    fake_random_push(&f.random, 10u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, 29u), RAFT89_OK);
    expect_no_action(node);
    CHECK_EQ(raft89_tick(node, 1u), RAFT89_OK);
    expect_election(node);
    raft89_destroy(node);

    /* T10: accumulated small ticks equal one combined tick. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    {
        unsigned long i;
        for (i = 0u; i < 9u; ++i)
        {
            CHECK_EQ(raft89_tick(node, 2u), RAFT89_OK);
            expect_no_action(node);
        }
        CHECK_EQ(raft89_tick(node, 2u), RAFT89_OK);
    }
    expect_election(node);
    raft89_destroy(node);

    /* T11: time saturates rather than wrapping or faulting. */
    fixture_init(&f, 3u, 1u);
    node = NULL;
    CHECK_EQ(raft89_create(&f.config, &node), RAFT89_OK);
    CHECK(node != NULL);
    CHECK_EQ(raft89_tick(node, ULONG_MAX), RAFT89_OK);
    expect_election(node);
    CHECK_EQ(raft89_tick(node, ULONG_MAX), RAFT89_OK);
    CHECK_EQ(driver_expect_hard_state(node, 2u, 1u), 1);
    driver_drain(node);
    {
        raft89_status status;
        CHECK_EQ(raft89_status_get(node, &status), RAFT89_OK);
        CHECK_EQ(status.faulted, 0);
    }
    raft89_destroy(node);

    TEST_END;
}
