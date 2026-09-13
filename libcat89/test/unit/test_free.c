/* cat89_test_free.c - free category over a small directed graph. */
#include <cat89/cat89.h>
#include <cat89/free.h>
#include <test.h>

int cat89_test_failures = 0;

struct edge
{
    int src;
    int dst;
    int id;
};

static int g_vertex_same(void *ctx, const void *a, const void *b)
{
    const int *x = a;
    const int *y = b;
    (void)ctx;
    return *x == *y;
}

static const void *g_edge_source(void *ctx, const void *edge)
{
    const struct edge *e = edge;
    (void)ctx;
    return &e->src;
}

static const void *g_edge_target(void *ctx, const void *edge)
{
    const struct edge *e = edge;
    (void)ctx;
    return &e->dst;
}

static void test_paths(void)
{
    cat89_category *cat = NULL;
    cat89_free_graph_ops ops;
    struct edge ef = {0, 1, 0};
    struct edge eg = {1, 2, 1};
    struct edge eh = {0, 2, 2};
    int va = 0;
    int vb = 1;
    int vc = 2;
    const cat89_obj *a = NULL;
    const cat89_obj *b = NULL;
    const cat89_obj *c = NULL;
    cat89_mor *f = NULL;
    cat89_mor *g = NULL;
    cat89_mor *h = NULL;
    cat89_mor *gf = NULL;
    cat89_mor *id_a = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;

    ops.vertex_same = g_vertex_same;
    ops.edge_source = g_edge_source;
    ops.edge_target = g_edge_target;
    T_STATUS(cat89_free_category_new(&ops, NULL, NULL, &cat), CAT89_OK);
    T_ASSERT(cat != NULL);

    T_STATUS(cat89_free_obj(cat, &va, &a), CAT89_OK);
    T_STATUS(cat89_free_obj(cat, &vb, &b), CAT89_OK);
    T_STATUS(cat89_free_obj(cat, &vc, &c), CAT89_OK);

    /* f: A -> B, g: B -> C, h: A -> C. */
    T_STATUS(cat89_free_edge(cat, &ef, &f), CAT89_OK);
    T_STATUS(cat89_free_edge(cat, &eg, &g), CAT89_OK);
    T_STATUS(cat89_free_edge(cat, &eh, &h), CAT89_OK);
    T_STATUS(cat89_dom(cat, f, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, f, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, b));

    /* composite g o f : A -> C, distinct from the direct edge h. */
    T_STATUS(cat89_compose(cat, g, f, &gf), CAT89_OK);
    T_STATUS(cat89_dom(cat, gf, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, gf, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, c));
    T_ASSERT(gf != h);

    /* identity is the empty path: id_A : A -> A. */
    T_STATUS(cat89_identity(cat, a, &id_a), CAT89_OK);
    T_STATUS(cat89_dom(cat, id_a, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, id_a, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, a));

    /* composing the composite further yields A -> C (still). */
    {
        cat89_mor *both = NULL;
        T_STATUS(cat89_compose(cat, gf, id_a, &both), CAT89_OK);
        T_STATUS(cat89_cod(cat, both, &cc), CAT89_OK);
        T_ASSERT(cat89_obj_same(cat, cc, c));
        cat89_mor_release(cat, both);
    }

    /* fallible calls clear *out on failure; never reuse an owned result here.
     */
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, h);
    cat89_mor_release(cat, gf);
    cat89_mor_release(cat, id_a);

    {
        cat89_mor *dummy = NULL;
        const cat89_obj *da = NULL;
        T_STATUS(cat89_free_edge(cat, NULL, &dummy), CAT89_INVALID);
        T_STATUS(cat89_free_obj(NULL, &va, &da), CAT89_INVALID);
    }
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_paths();
    return T_END() ? 0 : 1;
}
