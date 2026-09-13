#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

/* packet_queue.h - test-only deep-copied semantic message packets used
 * by the deterministic cluster simulator. Not part of the library. */

#include <raft89.h>

#define PACKET_ENTRY_MAX 8
#define PACKET_DATA_MAX 16

typedef struct packet
{
    raft89_id from;
    raft89_id to;
    raft89_message msg;
    raft89_entry entries[PACKET_ENTRY_MAX];
    unsigned char data[PACKET_ENTRY_MAX][PACKET_DATA_MAX];
} packet;

/* Deep-copy src into dst, repointing every entry payload at dst's own
 * storage. */
void packet_copy(packet *dst, const packet *src);

/* Copy an outstanding SEND action into a self-contained packet. Returns
 * 0 on success, -1 when the message does not fit the packet bounds. */
int packet_from_send(raft89_id from, const raft89_action *action, packet *out);

#endif /* PACKET_QUEUE_H */
