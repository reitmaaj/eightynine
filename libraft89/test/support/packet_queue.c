/* packet_queue.c - deep-copied semantic message packets. */
#include <string.h>

#include "packet_queue.h"

void packet_copy(packet *dst, const packet *src)
{
    unsigned long i;
    unsigned long count;
    *dst = *src;
    if (dst->msg.type != RAFT89_MSG_APPEND_ENTRIES)
    {
        return;
    }
    count = (unsigned long)src->msg.u.append_entries.entry_count;
    if (count > (unsigned long)PACKET_ENTRY_MAX)
    {
        count = (unsigned long)PACKET_ENTRY_MAX;
    }
    dst->msg.u.append_entries.entries = dst->entries;
    for (i = 0u; i < count; ++i)
    {
        dst->entries[i] = src->entries[i];
        if (src->entries[i].size == 0u)
        {
            dst->entries[i].data = NULL;
        }
        else
        {
            memcpy(dst->data[i], src->data[i], src->entries[i].size);
            dst->entries[i].data = dst->data[i];
        }
    }
}

int packet_from_send(raft89_id from, const raft89_action *action, packet *out)
{
    const raft89_entry *entry;
    unsigned long count;
    unsigned long i;
    memset(out, 0, sizeof(*out));
    out->from = from;
    out->to = action->u.send.message.to;
    out->msg = action->u.send.message;
    if (out->msg.type != RAFT89_MSG_APPEND_ENTRIES)
    {
        return 0;
    }
    count = (unsigned long)out->msg.u.append_entries.entry_count;
    if (count > (unsigned long)PACKET_ENTRY_MAX)
    {
        return -1;
    }
    out->msg.u.append_entries.entries = out->entries;
    for (i = 0u; i < count; ++i)
    {
        entry = &action->u.send.message.u.append_entries.entries[i];
        if (entry->size > (raft89_size)PACKET_DATA_MAX)
        {
            return -1;
        }
        out->entries[i] = *entry;
        if (entry->size == 0u)
        {
            out->entries[i].data = NULL;
        }
        else
        {
            memcpy(out->data[i], entry->data, entry->size);
            out->entries[i].data = out->data[i];
        }
    }
    return 0;
}
