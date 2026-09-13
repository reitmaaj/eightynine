/* test_random_sim.c - seeded random traces over the shared cluster with
 * invariant checking, trace printing, and replay. See
 * .agent/testing/0008-random-traces.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "invariants.h"
#include "prng.h"
#include "sim_event.h"

#define RANDOM_NODES 3
#define RANDOM_MAX_PACKETS 6ul
#define RANDOM_MAX_CRASHES 2ul

static const unsigned long event_weights[SIM_EV_TYPE_COUNT] = {
    3ul, /* TICK */
    8ul, /* DELIVER */
    1ul, /* DROP */
    1ul, /* DUP */
    2ul, /* CRASH0 */
    2ul, /* CRASH1 */
    3ul, /* RESTART */
    5ul, /* PROPOSE */
    1ul, /* LINK_UP */
    1ul, /* LINK_DOWN */
    8ul, /* EFFECT */
    8ul, /* ACK */
    1ul  /* LOST */
};

static int result_acceptable(int rc)
{
    if (rc == RAFT89_OK)
    {
        return 1;
    }
    if (rc == RAFT89_ERR_PROTOCOL)
    {
        return 1;
    }
    if (rc == RAFT89_NOT_LEADER)
    {
        return 1;
    }
    return 0;
}

static void print_trace(FILE *out, unsigned long seed, const sim_event *trace,
                        unsigned long count)
{
    unsigned long i;
    fprintf(out, "seed=%lu steps=%lu\n", seed, count);
    for (i = 0ul; i < count; ++i)
    {
        sim_event_print(out, &trace[i]);
    }
}

static sim_event *pick_event(prng *r, const sim_event *events,
                             unsigned long count)
{
    unsigned long total;
    unsigned long pick;
    unsigned long weight;
    unsigned long i;
    total = 0ul;
    for (i = 0ul; i < count; ++i)
    {
        total += event_weights[events[i].type];
    }
    pick = prng_below(r, total);
    for (i = 0ul; i < count; ++i)
    {
        weight = event_weights[events[i].type];
        if (pick < weight)
        {
            return (sim_event *)&events[i];
        }
        pick -= weight;
    }
    return (sim_event *)&events[count - 1ul];
}

static int run_one(unsigned long seed, unsigned long steps)
{
    cluster c;
    cluster prev;
    prng r;
    sim_event events[SIM_EVENT_MAX];
    sim_event *trace;
    sim_event chosen;
    const char *violation;
    unsigned long count;
    unsigned long step;
    int rc;
    trace = (sim_event *)malloc(steps * sizeof(sim_event));
    if (trace == NULL)
    {
        fprintf(stderr, "random: out of memory\n");
        return -1;
    }
    cluster_init(&c, RANDOM_NODES, 0ul);
    prng_seed(&r, seed);
    for (step = 0ul; step < steps; ++step)
    {
        count = sim_events_collect(&c, RANDOM_MAX_PACKETS, RANDOM_MAX_CRASHES,
                                   1, events, SIM_EVENT_MAX);
        if (count == 0ul)
        {
            break;
        }
        chosen = *pick_event(&r, events, count);
        trace[step] = chosen;
        if (cluster_clone(&c, &prev) != RAFT89_OK)
        {
            fprintf(stderr, "random: clone failed\n");
            free(trace);
            cluster_free(&c);
            return -1;
        }
        rc = sim_event_apply(&c, &chosen);
        if (result_acceptable(rc) == 0)
        {
            fprintf(stderr, "random: seed=%lu step=%lu apply error %d\n", seed,
                    step, rc);
            cluster_free(&prev);
            free(trace);
            cluster_free(&c);
            return -1;
        }
        violation = invariants_check(&c);
        if (violation == NULL)
        {
            violation = invariants_check_transition(&prev, &c);
        }
        cluster_free(&prev);
        if (violation != NULL)
        {
            fprintf(stderr, "RANDOM VIOLATION: %s\n", violation);
            print_trace(stderr, seed, trace, step + 1ul);
            free(trace);
            cluster_free(&c);
            return 1;
        }
    }
    free(trace);
    cluster_free(&c);
    return 0;
}

static int run_replay(const char *path)
{
    FILE *f;
    char line[256];
    cluster c;
    cluster prev;
    sim_event ev;
    const char *violation;
    unsigned long step;
    int rc;
    f = fopen(path, "r");
    if (f == NULL)
    {
        fprintf(stderr, "replay: cannot open %s\n", path);
        return 2;
    }
    cluster_init(&c, RANDOM_NODES, 0ul);
    step = 0ul;
    while (fgets(line, (int)sizeof(line), f) != NULL)
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        if (strncmp(line, "seed=", 5) == 0)
        {
            continue;
        }
        if (sim_event_parse(line, &ev) != 0)
        {
            fprintf(stderr, "replay: malformed event line %lu: %s", step, line);
            fclose(f);
            cluster_free(&c);
            return 2;
        }
        if (cluster_clone(&c, &prev) != RAFT89_OK)
        {
            fprintf(stderr, "replay: clone failed\n");
            fclose(f);
            cluster_free(&c);
            return 2;
        }
        rc = sim_event_apply(&c, &ev);
        if (result_acceptable(rc) == 0)
        {
            fprintf(stderr, "replay: event %lu does not apply: rc=%d\n", step,
                    rc);
            cluster_free(&prev);
            fclose(f);
            cluster_free(&c);
            return 1;
        }
        violation = invariants_check(&c);
        if (violation == NULL)
        {
            violation = invariants_check_transition(&prev, &c);
        }
        cluster_free(&prev);
        if (violation != NULL)
        {
            fprintf(stderr, "REPLAY VIOLATION: %s\n", violation);
            fclose(f);
            cluster_free(&c);
            return 1;
        }
        ++step;
    }
    fclose(f);
    cluster_free(&c);
    printf("replay steps=%lu result=pass\n", step);
    return 0;
}

int main(int argc, char **argv)
{
    unsigned long seed;
    unsigned long steps;
    unsigned long sweep;
    const char *replay;
    unsigned long s;
    int single;
    int i;
    int rc;
    seed = 1ul;
    steps = 300ul;
    sweep = 16ul;
    replay = NULL;
    single = 0;
    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
        {
            ++i;
            seed = strtoul(argv[i], NULL, 10);
            single = 1;
        }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
        {
            ++i;
            steps = strtoul(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "--sweep") == 0 && i + 1 < argc)
        {
            ++i;
            sweep = strtoul(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc)
        {
            ++i;
            replay = argv[i];
        }
        else
        {
            fprintf(stderr,
                    "usage: test_random_sim [--seed N] [--sweep N] [--steps N] "
                    "[--replay FILE]\n");
            return 2;
        }
    }
    if (replay != NULL)
    {
        return run_replay(replay);
    }
    if (single != 0)
    {
        rc = run_one(seed, steps);
        if (rc != 0)
        {
            return 1;
        }
        printf("seed=%lu steps=%lu result=pass\n", seed, steps);
        return 0;
    }
    for (s = 1ul; s <= sweep; ++s)
    {
        rc = run_one(s, steps);
        if (rc != 0)
        {
            return 1;
        }
    }
    printf("sweep=%lu steps=%lu result=pass\n", sweep, steps);
    return 0;
}
