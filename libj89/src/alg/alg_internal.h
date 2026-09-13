#ifndef J89_ALG_INTERNAL_H
#define J89_ALG_INTERNAL_H

#include <stddef.h>
#include <string.h>

#include "../../include/j89_alg.h"

/* Every refcounted node carries its own free callback and context so that
 * release can reclaim the node regardless of which allocator constructed it. */
typedef struct j89a_hdr
{
    j89a_refs refs;
    void (*freer)(void *ctx, void *p);
    void *fctx;
} j89a_hdr;

struct j89a_str
{
    j89a_hdr hdr;
    size_t len;
    char data[1]; /* C89 flexible-tail idiom; data[len] == '\0' */
};

#define J89A_LIST_NIL 0
#define J89A_LIST_CONS 1

/* Representation fields store non-const pointers to independently owned,
 * refcounted children. A const container still yields a non-const child
 * pointer when read (C const protects the field, not the pointee), which is
 * what lets retain/release and sharing constructors operate without a cast. */
struct j89a_list
{
    j89a_hdr hdr;
    int tag;
    struct j89a_json *head;
    struct j89a_list *tail;
};

#define J89A_OBJECT_EMPTY 0
#define J89A_OBJECT_MEMBER 1
#define J89A_OBJECT_UNION 2

struct j89a_object
{
    j89a_hdr hdr;
    int tag;
    struct j89a_str *name;     /* MEMBER */
    struct j89a_json *value;   /* MEMBER */
    struct j89a_object *left;  /* UNION */
    struct j89a_object *right; /* UNION */
};

struct j89a_json
{
    j89a_hdr hdr;
    j89a_kind kind;
    union
    {
        j89a_bool b;
        double n;
        struct j89a_str *s;
        struct j89a_list *l;
        struct j89a_object *o;
    } u;
};

/* Fold-local R containers. An R value lives in its own buffer allocated from
 * the operation's allocator (max-aligned like malloc). */
struct j89a_rlist
{
    struct j89a_rlist *next;
    void *r;
};

struct j89a_robject
{
    struct j89a_robject *left;
    struct j89a_robject *right;
    const struct j89a_str *name; /* MEMBER or left leaf */
    void *r;
    int tag; /* EMPTY / MEMBER / UNION */
};

#define J89A_ROBJ_EMPTY 0
#define J89A_ROBJ_MEMBER 1
#define J89A_ROBJ_UNION 2

/* ---- allocator helpers ----------------------------------------------------
 */

void *j89a_malloc(j89a_alloc *al,
                  size_t size); /* records failed, returns NULL */
void j89a_nfree(void (*freer)(void *, void *), void *fctx, void *p);

/* ---- R-runtime helpers ----------------------------------------------------
 */

j89a_status j89a_rcopy(const j89a_rtype *rt, void *dst, const void *src);
void j89a_rdrop(const j89a_rtype *rt, void *val);

/* ---- node retain / release helpers ----------------------------------------
 */

void j89a_list_node_free(struct j89a_list *xs);
void j89a_object_node_free(struct j89a_object *o);
void j89a_json_node_free(struct j89a_json *value);

#endif
