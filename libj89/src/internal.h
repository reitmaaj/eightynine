#ifndef J89_INTERNAL_H
#define J89_INTERNAL_H

#include "../include/j89.h"
#include "str89.h"

/* Internal node and member layouts. Nodes and member slots are stored as
 * byte offsets into the single arena block, so a node index doubles as a
 * byte offset.
 *
 * String bytes and object keys are owned str89 values. A string node and an
 * object member store a registry offset; the registry node (allocated in the
 * arena block) holds the str89. Offset 0 of the block is the registry head;
 * J89_BAD means the registry is empty. The arena owns every registered str89
 * and releases them in j89_arena_destroy. */

typedef struct j89_node {
    j89_kind kind;
    j89_len n;      /* string bytes / array len / object member count */
    j89_len base;   /* string byte offset / first child slot / first member */
    j89_int iv;     /* integer value when kind == J89_INTEGER */
    double dv;      /* double value when kind == J89_FLOAT */
} j89_node;

typedef struct j89_member {
    j89_len ko;     /* key string byte offset */
    j89_len kl;     /* key string byte length */
    j89_len value;  /* value node index */
} j89_member;

typedef union j89_align {
    long l;
    size_t s;
    void *p;
} j89_align;

#define J89_ALIGN (sizeof(j89_align))

/* arena */
j89_len j89_alloc(j89_arena *a, j89_len size);
void *j89_ptr(j89_arena *a, j89_len off);
int j89_is_bad(j89_len x);
void j89_set_error(j89_arena *a, const char *msg);

/* node field accessors */
j89_kind j89_node_kind(j89_arena *a, j89_len node);
j89_len j89_node_n(j89_arena *a, j89_len node);
j89_len j89_node_base(j89_arena *a, j89_len node);
j89_int j89_node_iv(j89_arena *a, j89_len node);
double j89_node_dv(j89_arena *a, j89_len node);
void j89_node_set_kind(j89_arena *a, j89_len node, j89_kind kind);
void j89_node_set_n(j89_arena *a, j89_len node, j89_len n);
void j89_node_set_base(j89_arena *a, j89_len node, j89_len base);
void j89_node_set_iv(j89_arena *a, j89_len node, j89_int iv);
void j89_node_set_dv(j89_arena *a, j89_len node, double dv);

/* construction */
j89_len j89_new_node(j89_arena *a);
j89_len j89_add_string(j89_arena *a, const char *s, j89_len len);
j89_len j89_string_node(j89_arena *a, const str89 *owned);
j89_len j89_add_int(j89_arena *a, j89_int v);
j89_len j89_add_double(j89_arena *a, double v);
j89_len j89_make_array(j89_arena *a, j89_len count);
j89_len j89_make_object(j89_arena *a, j89_len count);
void j89_array_set_child(j89_arena *a, j89_len arr, j89_len i, j89_len child);
void j89_object_set_member(j89_arena *a, j89_len obj, j89_len i,
                           j89_len ko, j89_len kl, j89_len value);

/* owned-string registry */
j89_len j89_str_register(j89_arena *a, const str89 *s);
const str89 *j89_str_at(j89_arena *a, j89_len off);
const char *j89_str_bytes(j89_arena *a, j89_len off);
j89_len j89_str_len(j89_arena *a, j89_len off);
void j89_str_release_all(j89_arena *a);

/* array / object member reads */
j89_len j89_child_id(j89_arena *a, j89_len base, j89_len i);
j89_len j89_member_ko(j89_arena *a, j89_len base, j89_len i);
j89_len j89_member_kl(j89_arena *a, j89_len base, j89_len i);
j89_len j89_member_value(j89_arena *a, j89_len base, j89_len i);

/* parse entry */
j89_len j89_parse_bytes(const char *buf, j89_len len, j89_arena *a);

#endif
