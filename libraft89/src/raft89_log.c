/* raft89_log.c - log entry copying, commit advancement, and ordered
 * application of committed entries. */
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "raft89_internal.h"

raft89_index raft89__min_index(raft89_index a, raft89_index b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

static unsigned long payload_total(const raft89_entry *src, raft89_size count)
{
    unsigned long total;
    raft89_size i;
    total = 0u;
    for (i = 0u; i < count; ++i)
    {
        total = raft89__sat_add(total, src[i].size);
    }
    return total;
}

int raft89__log_term_at(raft89 *node, raft89_index index, raft89_term *term)
{
    int rc;
    if (index == RAFT89_INDEX_NONE)
    {
        *term = RAFT89_TERM_NONE;
        return RAFT89_OK;
    }
    rc = node->store.log_term(node->store.ctx, index, term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    return RAFT89_OK;
}

unsigned char *raft89__entries_buffer_alloc(raft89_size count,
                                            unsigned long total)
{
    unsigned long desc;
    unsigned char *buffer;
    if (count > ULONG_MAX / sizeof(raft89_entry))
    {
        return NULL;
    }
    desc = count * sizeof(raft89_entry);
    if (total > ULONG_MAX - desc)
    {
        return NULL;
    }
    buffer = (unsigned char *)malloc(desc + total);
    return buffer;
}

static void entry_store(raft89_entry *dst, unsigned char *data,
                        unsigned long *offset, const raft89_entry *src)
{
    dst->term = src->term;
    dst->index = src->index;
    dst->size = src->size;
    dst->data = data + *offset;
    if (src->size != 0u)
    {
        memcpy(data + *offset, src->data, src->size);
    }
    *offset += src->size;
}

void raft89__entries_clear(raft89 *node)
{
    free(node->pending_entries);
    node->pending_entries = NULL;
    node->pending_count = 0u;
}

int raft89__entries_stash(raft89 *node, const raft89_entry *src,
                          raft89_size count)
{
    unsigned long total;
    unsigned long offset;
    unsigned char *base;
    raft89_entry *entries;
    raft89_size i;
    raft89__entries_clear(node);
    if (count == 0u)
    {
        return RAFT89_OK;
    }
    total = payload_total(src, count);
    base = raft89__entries_buffer_alloc(count, total);
    if (base == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    entries = (raft89_entry *)base;
    offset = count * sizeof(raft89_entry);
    for (i = 0u; i < count; ++i)
    {
        entry_store(&entries[i], base, &offset, &src[i]);
    }
    node->pending_entries = entries;
    node->pending_count = count;
    return RAFT89_OK;
}

static int load_entry_meta(raft89 *node, raft89_entry *entry)
{
    int rc;
    raft89_term term;
    raft89_size size;
    rc = node->store.log_term(node->store.ctx, entry->index, &term);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    rc = node->store.log_size(node->store.ctx, entry->index, &size);
    if (rc != RAFT89_OK)
    {
        node->faulted = 1;
        return RAFT89_ERR_STORE;
    }
    entry->term = term;
    entry->size = size;
    return RAFT89_OK;
}

static int store_fault(raft89 *node, void *buffer)
{
    free(buffer);
    node->faulted = 1;
    return RAFT89_ERR_STORE;
}

static void apply_buffer_discard(raft89 *node)
{
    free(node->apply_buffer);
    node->apply_buffer = NULL;
    node->apply_size = 0u;
}

static int load_entry_payload(raft89 *node, raft89_entry *entry)
{
    int rc;
    void *buffer;
    if (entry->size == 0u)
    {
        entry->data = NULL;
        return RAFT89_OK;
    }
    buffer = malloc(entry->size);
    if (buffer == NULL)
    {
        return RAFT89_ERR_NOMEM;
    }
    rc = node->store.log_read(node->store.ctx, entry->index, buffer,
                              entry->size);
    if (rc != RAFT89_OK)
    {
        rc = store_fault(node, buffer);
        return rc;
    }
    node->apply_buffer = buffer;
    node->apply_size = entry->size;
    entry->data = buffer;
    return RAFT89_OK;
}

static int emit_next_apply(raft89 *node)
{
    int rc;
    raft89_entry entry;
    entry.index = node->applied_index + 1u;
    entry.term = RAFT89_TERM_NONE;
    entry.data = NULL;
    entry.size = 0u;
    rc = load_entry_meta(node, &entry);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = load_entry_payload(node, &entry);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    rc = raft89__action_begin(node, RAFT89_ACT_APPLY);
    if (rc != RAFT89_OK)
    {
        apply_buffer_discard(node);
        return rc;
    }
    node->action.u.apply.entry = entry;
    return RAFT89_OK;
}

int raft89__apply_next(raft89 *node)
{
    int rc;
    if (node->applied_index >= node->commit_index)
    {
        return RAFT89_OK;
    }
    rc = emit_next_apply(node);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    node->step = RAFT89_STEP_APPLY;
    return RAFT89_OK;
}

int raft89__apply_ack(raft89 *node)
{
    int rc;
    enum raft89_step after;
    apply_buffer_discard(node);
    ++node->applied_index;
    after = node->after_apply;
    node->step = RAFT89_STEP_NONE;
    rc = raft89__apply_next(node);
    if (rc != RAFT89_OK)
    {
        return rc;
    }
    if (node->step == RAFT89_STEP_APPLY)
    {
        return RAFT89_OK;
    }
    node->step = after;
    node->after_apply = RAFT89_STEP_NONE;
    rc = raft89__resume(node);
    return rc;
}

int raft89__advance_commit(raft89 *node, raft89_index new_commit,
                           enum raft89_step after)
{
    int rc;
    if (new_commit <= node->commit_index)
    {
        return RAFT89_OK;
    }
    node->commit_index = new_commit;
    node->after_apply = after;
    rc = raft89__apply_next(node);
    return rc;
}
