/* sim_event.c - shared simulator event layer. */
#include <string.h>

#include "raft89_inspect.h"
#include "sim_event.h"

static const unsigned long tick_values[3] = {20ul, 10ul, 1ul};

static const char *const event_names[SIM_EV_TYPE_COUNT] = {
    "TICK",    "DELIVER", "DROP",     "DUP",    "CRASH0", "CRASH1", "RESTART",
    "PROPOSE", "LINKUP",  "LINKDOWN", "EFFECT", "ACK",    "LOST"};

static void add_event(sim_event *out, unsigned long *count, unsigned long max,
                      unsigned long type, raft89_id a, raft89_id b,
                      unsigned long u, unsigned char cmd)
{
    if (*count >= max)
    {
        return;
    }
    out[*count].type = type;
    out[*count].a = a;
    out[*count].b = b;
    out[*count].u = u;
    out[*count].cmd = cmd;
    ++(*count);
}

unsigned long sim_events_collect(const cluster *c, unsigned long max_packets,
                                 unsigned long max_crashes, int env,
                                 sim_event *out, unsigned long max)
{
    const cluster_node *n;
    const raft89_action *action;
    raft89_id a;
    raft89_id b;
    unsigned long i;
    unsigned long k;
    unsigned long p;
    unsigned long count;
    count = 0ul;
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (n->up != 0 && n->raft != NULL)
        {
            if (n->oracle.phase == ORACLE_ISSUED)
            {
                add_event(out, &count, max, SIM_EV_EFFECT, n->f.config.self,
                          RAFT89_ID_NONE, 0ul, 0u);
            }
            if (n->oracle.phase == ORACLE_EFFECT_DONE)
            {
                add_event(out, &count, max, SIM_EV_ACK, n->f.config.self,
                          RAFT89_ID_NONE, 0ul, 0u);
            }
        }
        else if (n->up == 0)
        {
            add_event(out, &count, max, SIM_EV_RESTART, n->f.config.self,
                      RAFT89_ID_NONE, 0ul, 0u);
        }
    }
    for (p = 0ul; p < c->queue_count; ++p)
    {
        if (cluster_deliverable(c, p) != 0)
        {
            add_event(out, &count, max, SIM_EV_DELIVER, RAFT89_ID_NONE,
                      RAFT89_ID_NONE, p, 0u);
        }
    }
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (n->up == 0 || n->raft == NULL)
        {
            continue;
        }
        if (n->oracle.phase != ORACLE_IDLE)
        {
            continue;
        }
        add_event(out, &count, max, SIM_EV_PROPOSE, n->f.config.self,
                  RAFT89_ID_NONE, 0ul, 'A');
        add_event(out, &count, max, SIM_EV_PROPOSE, n->f.config.self,
                  RAFT89_ID_NONE, 0ul, 'B');
    }
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (n->up == 0 || n->raft == NULL)
        {
            continue;
        }
        if (n->oracle.phase != ORACLE_IDLE)
        {
            continue;
        }
        for (k = 0ul; k < 3ul; ++k)
        {
            add_event(out, &count, max, SIM_EV_TICK, n->f.config.self,
                      RAFT89_ID_NONE, tick_values[k], 0u);
        }
    }
    if (env == 0)
    {
        return count;
    }
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (n->up == 0 || n->raft == NULL)
        {
            continue;
        }
        if (n->oracle.phase != ORACLE_ISSUED)
        {
            continue;
        }
        action = raft89_inspect_action(n->raft);
        if (action != NULL && action->type == RAFT89_ACT_SEND)
        {
            add_event(out, &count, max, SIM_EV_LOST, n->f.config.self,
                      RAFT89_ID_NONE, 0ul, 0u);
        }
    }
    for (p = 0ul; p < c->queue_count; ++p)
    {
        if (c->queue_count < max_packets)
        {
            add_event(out, &count, max, SIM_EV_DUP, RAFT89_ID_NONE,
                      RAFT89_ID_NONE, p, 0u);
        }
        add_event(out, &count, max, SIM_EV_DROP, RAFT89_ID_NONE, RAFT89_ID_NONE,
                  p, 0u);
    }
    for (a = 1ul; a <= (raft89_id)c->count; ++a)
    {
        for (b = 1ul; b <= (raft89_id)c->count; ++b)
        {
            if (a == b)
            {
                continue;
            }
            if (c->link[a][b] != 0)
            {
                add_event(out, &count, max, SIM_EV_LINK_DOWN, a, b, 0ul, 0u);
            }
            else
            {
                add_event(out, &count, max, SIM_EV_LINK_UP, a, b, 0ul, 0u);
            }
        }
    }
    for (i = 0ul; i < (unsigned long)c->count; ++i)
    {
        n = &c->nodes[i];
        if (n->up == 0 || n->raft == NULL)
        {
            continue;
        }
        if (n->crashes >= max_crashes)
        {
            continue;
        }
        add_event(out, &count, max, SIM_EV_CRASH0, n->f.config.self,
                  RAFT89_ID_NONE, 0ul, 0u);
        add_event(out, &count, max, SIM_EV_CRASH1, n->f.config.self,
                  RAFT89_ID_NONE, 0ul, 0u);
    }
    return count;
}

int sim_event_apply(cluster *c, const sim_event *ev)
{
    unsigned char cmd;
    if (ev->type == SIM_EV_TICK)
    {
        return cluster_tick(c, ev->a, ev->u);
    }
    if (ev->type == SIM_EV_DELIVER)
    {
        return cluster_deliver(c, ev->u);
    }
    if (ev->type == SIM_EV_DROP)
    {
        cluster_drop(c, ev->u);
        return RAFT89_OK;
    }
    if (ev->type == SIM_EV_DUP)
    {
        return cluster_duplicate(c, ev->u);
    }
    if (ev->type == SIM_EV_CRASH0)
    {
        return cluster_crash(c, ev->a, 0);
    }
    if (ev->type == SIM_EV_CRASH1)
    {
        return cluster_crash(c, ev->a, 1);
    }
    if (ev->type == SIM_EV_RESTART)
    {
        return cluster_restart(c, ev->a);
    }
    if (ev->type == SIM_EV_PROPOSE)
    {
        cmd = ev->cmd;
        return cluster_propose(c, ev->a, &cmd, 1ul);
    }
    if (ev->type == SIM_EV_LINK_UP)
    {
        cluster_link(c, ev->a, ev->b, 1);
        return RAFT89_OK;
    }
    if (ev->type == SIM_EV_LINK_DOWN)
    {
        cluster_link(c, ev->a, ev->b, 0);
        return RAFT89_OK;
    }
    if (ev->type == SIM_EV_EFFECT)
    {
        return cluster_effect(c, ev->a);
    }
    if (ev->type == SIM_EV_ACK)
    {
        return cluster_ack(c, ev->a);
    }
    return cluster_send_lost(c, ev->a);
}

const char *sim_event_name(unsigned long type)
{
    if (type >= (unsigned long)SIM_EV_TYPE_COUNT)
    {
        return "?";
    }
    return event_names[type];
}

void sim_event_print(FILE *out, const sim_event *ev)
{
    fprintf(out, "event %s %lu %lu %lu", sim_event_name(ev->type), ev->a, ev->b,
            ev->u);
    if (ev->type == SIM_EV_PROPOSE)
    {
        fprintf(out, " %c", (char)ev->cmd);
    }
    fprintf(out, "\n");
}

static int type_from_name(const char *name)
{
    unsigned long i;
    for (i = 0ul; i < (unsigned long)SIM_EV_TYPE_COUNT; ++i)
    {
        if (strcmp(name, event_names[i]) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

int sim_event_parse(const char *line, sim_event *ev)
{
    char name[16];
    char cmd;
    unsigned long a;
    unsigned long b;
    unsigned long u;
    int type;
    int n;
    cmd = 0;
    n = sscanf(line, "event %15s %lu %lu %lu %c", name, &a, &b, &u, &cmd);
    if (n < 4)
    {
        return -1;
    }
    type = type_from_name(name);
    if (type < 0)
    {
        return -1;
    }
    if (type == (int)SIM_EV_PROPOSE)
    {
        if (n < 5)
        {
            return -1;
        }
    }
    else
    {
        cmd = 0;
    }
    ev->type = (unsigned long)type;
    ev->a = (raft89_id)a;
    ev->b = (raft89_id)b;
    ev->u = u;
    ev->cmd = (unsigned char)cmd;
    return 0;
}
