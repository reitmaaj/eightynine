/* raft89_timer.c - logical time accounting and timeout handling. */
#include <limits.h>
#include <stddef.h>

#include "raft89_internal.h"

unsigned long raft89__sat_add(unsigned long a, unsigned long b)
{
    if (ULONG_MAX - a < b)
    {
        return ULONG_MAX;
    }
    return a + b;
}

raft89_time raft89__pick_timeout(raft89 *node)
{
    unsigned long span;
    unsigned long pick;
    span = node->election_timeout_max - node->election_timeout_min + 1u;
    pick = node->random.next(node->random.ctx, span);
    return node->election_timeout_min + pick;
}

void raft89__advance_clocks(raft89 *node, raft89_time elapsed)
{
    node->election_elapsed = raft89__sat_add(node->election_elapsed, elapsed);
    node->heartbeat_elapsed = raft89__sat_add(node->heartbeat_elapsed, elapsed);
}

int raft89__tick_leader(raft89 *node)
{
    int rc;
    if (node->heartbeat_elapsed < node->heartbeat_interval)
    {
        return RAFT89_OK;
    }
    node->heartbeat_elapsed = 0u;
    node->peer_cursor = 0u;
    node->step = RAFT89_STEP_LEADER_BROADCAST;
    rc = raft89__leader_broadcast_next(node);
    return rc;
}

int raft89__tick_follower(raft89 *node)
{
    int rc;
    if (node->election_elapsed < node->election_timeout)
    {
        return RAFT89_OK;
    }
    rc = raft89__start_election(node);
    return rc;
}

int raft89_tick(raft89 *node, raft89_time elapsed)
{
    int rc;
    if (node == NULL)
    {
        return RAFT89_ERR_ARG;
    }
    if (node->faulted != 0)
    {
        return RAFT89_ERR_FAULTED;
    }
    if (node->has_action != 0)
    {
        return RAFT89_BUSY;
    }
    raft89__advance_clocks(node, elapsed);
    if (node->role == RAFT89_LEADER)
    {
        rc = raft89__tick_leader(node);
        return rc;
    }
    rc = raft89__tick_follower(node);
    return rc;
}
