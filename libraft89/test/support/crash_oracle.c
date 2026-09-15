/* crash_oracle.c - crash-point oracle. */
#include <stddef.h>

#include "crash_oracle.h"

static int finish(enum oracle_phase *phase, int rc);

void oracle_init(crash_oracle *o, raft89 *raft)
{
    o->raft = raft;
    o->phase = ORACLE_IDLE;
    o->action = NULL;
    o->incarnation = 0u;
}

int oracle_peek(crash_oracle *o)
{
    const raft89_action *action;
    int rc;
    action = NULL;
    rc = raft89_next_action(o->raft, &action);
    if (rc != RAFT89_OK)
    {
        o->phase = ORACLE_IDLE;
        o->action = NULL;
        return 0;
    }
    o->phase = ORACLE_ISSUED;
    o->action = action;
    return 1;
}

static unsigned long oracle_lo(raft89_u64 v)
{
    return (unsigned long)v.lo;
}

static int store_entries(fake_store *store, const raft89_entry *entries,
                         unsigned long count)
{
    unsigned long i;
    const raft89_entry *entry;
    for (i = 0u; i < count; ++i)
    {
        entry = &entries[i];
        if (fake_store_put(store, oracle_lo(entry->term),
                           oracle_lo(entry->index), entry->data,
                           entry->size) != RAFT89_OK)
        {
            return -1;
        }
    }
    return 0;
}

static int effect_append(crash_oracle *o, fake_store *store,
                         unsigned long count)
{
    const raft89_action *action;
    action = o->action;
    if (action->type != RAFT89_ACT_LOG_APPEND)
    {
        return -1;
    }
    if (count > action->u.log_append.entry_count)
    {
        return -1;
    }
    return store_entries(store, action->u.log_append.entries, count);
}

static int effect_truncate(crash_oracle *o, fake_store *store,
                           unsigned long boundary)
{
    const raft89_action *action;
    unsigned long first;
    action = o->action;
    if (action->type != RAFT89_ACT_LOG_TRUNCATE)
    {
        return -1;
    }
    first = oracle_lo(action->u.log_truncate.first_index);
    if (boundary < first - 1u)
    {
        return -1;
    }
    if (boundary > store->entry_count)
    {
        return -1;
    }
    fake_store_truncate(store, boundary + 1u);
    return 0;
}

static int effect_hard(crash_oracle *o, fake_store *store, int use_new)
{
    const raft89_action *action;
    action = o->action;
    if (action->type != RAFT89_ACT_HARD_STATE)
    {
        return -1;
    }
    if (use_new != 0)
    {
        store->hard = action->u.hard_state.state;
    }
    return 0;
}

static int effect_apply(crash_oracle *o, fake_app *app, int apply)
{
    const raft89_action *action;
    action = o->action;
    if (action->type != RAFT89_ACT_APPLY)
    {
        return -1;
    }
    if (apply == 0)
    {
        return 0;
    }
    return fake_app_apply(app, &action->u.apply.entry);
}

static int finish(enum oracle_phase *phase, int rc)
{
    if (rc != 0)
    {
        return rc;
    }
    *phase = ORACLE_EFFECT_DONE;
    return 0;
}

int oracle_effect_full(crash_oracle *o, fake_store *store, fake_app *app)
{
    int rc;
    rc = 0;
    if (o->action->type == RAFT89_ACT_HARD_STATE)
    {
        rc = effect_hard(o, store, 1);
    }
    if (o->action->type == RAFT89_ACT_LOG_APPEND)
    {
        rc = effect_append(o, store, o->action->u.log_append.entry_count);
    }
    if (o->action->type == RAFT89_ACT_LOG_TRUNCATE)
    {
        rc = effect_truncate(
            o, store, oracle_lo(o->action->u.log_truncate.first_index) - 1u);
    }
    if (o->action->type == RAFT89_ACT_APPLY)
    {
        rc = effect_apply(o, app, 1);
    }
    return finish(&o->phase, rc);
}

int oracle_effect_append_partial(crash_oracle *o, fake_store *store,
                                 unsigned long count)
{
    int rc;
    rc = effect_append(o, store, count);
    return finish(&o->phase, rc);
}

int oracle_effect_truncate_partial(crash_oracle *o, fake_store *store,
                                   unsigned long boundary)
{
    int rc;
    rc = effect_truncate(o, store, boundary);
    return finish(&o->phase, rc);
}

int oracle_effect_hard_partial(crash_oracle *o, fake_store *store, int use_new)
{
    int rc;
    rc = effect_hard(o, store, use_new);
    return finish(&o->phase, rc);
}

int oracle_effect_apply_partial(crash_oracle *o, fake_app *app, int apply)
{
    int rc;
    rc = effect_apply(o, app, apply);
    return finish(&o->phase, rc);
}

int oracle_ack(crash_oracle *o, enum raft89_action_result result)
{
    int rc;
    rc = raft89_action_done(o->raft, o->action->id, result);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    o->phase = ORACLE_ACKED;
    o->action = NULL;
    o->phase = ORACLE_IDLE;
    return RAFT89_OK;
}

void oracle_crash(crash_oracle *o)
{
    raft89_destroy(o->raft);
    o->raft = NULL;
    o->phase = ORACLE_IDLE;
    o->action = NULL;
    ++o->incarnation;
}

int oracle_restart(crash_oracle *o, const raft89_config *config)
{
    o->raft = NULL;
    return raft89_create(config, &o->raft);
}
