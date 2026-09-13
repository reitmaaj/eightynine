#ifndef HM_INTERNAL_H
#define HM_INTERNAL_H

/* hm_internal.h - private representation and cross-module helpers.
 * Included only by the library translation units under src/. */

#include <hm.h>

/* Raw allocation block: the header immediately precedes its payload. */
struct hm_blk
{
    struct hm_blk *next;
};

struct hm_type
{
    int kind;

    union
    {
        struct
        {
            unsigned long id;
            struct hm_type *link;
        } var;

        struct
        {
            char *name;
            size_t arity;
            struct hm_type **args;
        } con;
    } u;
};

struct hm_scheme
{
    size_t count;
    struct hm_type **vars;
    struct hm_type *body;
};

struct hm_binding
{
    char *name;
    struct hm_scheme *scheme;
    struct hm_binding *next;
};

struct hm_env
{
    hm_ctx *ctx;
    struct hm_env *parent;
    struct hm_binding *bindings;
};

struct hm_error
{
    int kind;
    const char *message;
    struct hm_type *left;
    struct hm_type *right;
    char *name;
    void *origin;
};

struct hm_ctx
{
    hm_allocator alloc;
    unsigned long next_var_id;
    struct hm_blk *blocks;
    void *origin;
};

/* Free-variable set, used by generalization and environment ftv. */
struct hm_i_set
{
    size_t len;
    size_t cap;
    struct hm_type **items;
};

/* Allocation. Returns zeroed payload from the context arena, or NULL. */
void *hm_i_alloc(hm_ctx *ctx, size_t size);

/* Null-terminated copy of src in the context arena, or NULL. */
char *hm_i_strdup(hm_ctx *ctx, const char *src);

/* Error node with kind, message, captured origin; or NULL on no-memory. */
struct hm_error *hm_i_error(hm_ctx *ctx, int kind, const char *message);

/* A zero-initialized type node in the context arena, or NULL. */
struct hm_type *hm_i_type_node(hm_ctx *ctx);

/* Free-variable set routines. */
struct hm_i_set *hm_i_set_new(hm_ctx *ctx);
int hm_i_set_has(const struct hm_i_set *set, const struct hm_type *v);
void hm_i_set_add(hm_ctx *ctx, struct hm_i_set *set, struct hm_type *v);
size_t hm_i_set_count(const struct hm_i_set *set);
struct hm_type *hm_i_set_at(const struct hm_i_set *set, size_t i);

/* Collect the free (unbound, pruned) variables of a type / an environment. */
void hm_i_type_ftv(hm_ctx *ctx, hm_type *type, struct hm_i_set *out);
void hm_i_env_ftv(hm_ctx *ctx, hm_env *env, struct hm_i_set *out);

/* True when var is quantified by scheme. */
int hm_i_var_quantified(const struct hm_type *var,
                        const struct hm_scheme *scheme);

#endif /* HM_INTERNAL_H */
