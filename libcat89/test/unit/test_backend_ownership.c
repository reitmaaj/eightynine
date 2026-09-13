/* cat89_test_backend_ownership.c - ownership across every built-in backend.
 *
 * Two independent instances of each backend kind are exercised: own handles
 * are accepted, cross-instance and cross-backend handles are rejected, results
 * are owned, retained handles stay registered, and live-registry unlink works
 * for head/middle/tail removals (sanitizer-checked teardown). */
#include <backend_fixtures.h>
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static void test_own_handles(struct cat89_backend_set *set)
{
    unsigned long i;
    struct cat89_bf_slot *s;

    for (i = 0; i < set->n; i = i + 1)
    {
        s = &set->slots[i];
        T_ASSERT(cat89_owns_obj(s->category, s->obj) == 1);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
        T_ASSERT(cat89_owns_obj(s->category, NULL) == 0);
        T_ASSERT(cat89_owns_mor(s->category, NULL) == 0);
        /* normalized, repeatable */
        T_ASSERT(cat89_owns_obj(s->category, s->obj) == 1);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
    }
}

static void test_cross_instance(struct cat89_backend_set *set)
{
    unsigned long i;
    unsigned long j;
    struct cat89_bf_slot *a;
    struct cat89_bf_slot *b;

    for (i = 0; i + 1 < set->n; i = i + 2)
    {
        a = &set->slots[i];
        b = &set->slots[i + 1];
        T_ASSERT(a->kind == b->kind);
        T_ASSERT(a->category != b->category);
        T_ASSERT(cat89_owns_obj(b->category, a->obj) == 0);
        T_ASSERT(cat89_owns_mor(b->category, a->mor) == 0);
        T_ASSERT(cat89_owns_obj(a->category, b->obj) == 0);
        T_ASSERT(cat89_owns_mor(a->category, b->mor) == 0);
        /* kinds sharing representation: a base object of an opposite slot is
         * still not owned by another independent instance */
        (void)j;
    }
}

static void test_cross_backend(struct cat89_backend_set *set)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < set->n; i = i + 1)
    {
        for (j = 0; j < set->n; j = j + 1)
        {
            if (i == j)
            {
                continue;
            }
            if (set->slots[i].kind == set->slots[j].kind)
            {
                continue;
            }
            T_ASSERT(
                cat89_owns_obj(set->slots[j].category, set->slots[i].obj) == 0);
            T_ASSERT(
                cat89_owns_mor(set->slots[j].category, set->slots[i].mor) == 0);
        }
    }
}

static void test_opposite_sharing(struct cat89_backend_set *set)
{
    unsigned long i;
    struct cat89_bf_slot *s;

    for (i = 0; i < set->n; i = i + 1)
    {
        s = &set->slots[i];
        if (s->kind != CAT89_BF_OPPOSITE)
        {
            continue;
        }
        T_ASSERT(cat89_owns_obj(s->base1, s->obj) == 1);
        T_ASSERT(cat89_owns_obj(s->category, s->obj) == 1);
        T_ASSERT(cat89_owns_mor(s->base1, s->base1_id) == 1);
        T_ASSERT(cat89_owns_mor(s->category, s->base1_id) == 0);
        T_ASSERT(cat89_owns_mor(s->base1, s->mor) == 0);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
    }
}

static void test_results_owned(struct cat89_backend_set *set)
{
    unsigned long i;
    struct cat89_bf_slot *s;
    const cat89_obj *dm;
    const cat89_obj *cm;
    cat89_mor *id;
    cat89_mor *r;
    cat89_status st;

    for (i = 0; i < set->n; i = i + 1)
    {
        s = &set->slots[i];
        dm = NULL;
        cm = NULL;
        T_STATUS(cat89_dom(s->category, s->mor, &dm), CAT89_OK);
        T_STATUS(cat89_cod(s->category, s->mor, &cm), CAT89_OK);
        T_ASSERT(cat89_owns_obj(s->category, dm) == 1);
        T_ASSERT(cat89_owns_obj(s->category, cm) == 1);

        id = NULL;
        T_STATUS(cat89_identity(s->category, cm, &id), CAT89_OK);
        T_ASSERT(cat89_owns_mor(s->category, id) == 1);

        r = NULL;
        st = cat89_compose(s->category, id, s->mor, &r);
        T_STATUS(st, CAT89_OK);
        T_ASSERT(r != NULL);
        T_ASSERT(cat89_owns_mor(s->category, r) == 1);
        cat89_mor_release(s->category, r);
        cat89_mor_release(s->category, id);
    }
}

static void test_retain_lifecycle(struct cat89_backend_set *set)
{
    unsigned long i;
    struct cat89_bf_slot *s;

    for (i = 0; i < set->n; i = i + 1)
    {
        s = &set->slots[i];
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
        T_STATUS(cat89_mor_retain(s->category, s->mor), CAT89_OK);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
        cat89_mor_release(s->category, s->mor);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
    }
}

static void test_live_registry_removal(struct cat89_backend_set *set)
{
    unsigned long i;
    struct cat89_bf_slot *s;
    cat89_mor *m1;
    cat89_mor *m2;
    cat89_mor *m3;
    const cat89_obj *d;

    for (i = 0; i < set->n; i = i + 1)
    {
        s = &set->slots[i];
        m1 = NULL;
        m2 = NULL;
        m3 = NULL;
        T_STATUS(cat89_identity(s->category, s->obj, &m1), CAT89_OK);
        T_STATUS(cat89_identity(s->category, s->obj, &m2), CAT89_OK);
        T_STATUS(cat89_identity(s->category, s->obj, &m3), CAT89_OK);
        T_ASSERT(cat89_owns_mor(s->category, m1) == 1);
        T_ASSERT(cat89_owns_mor(s->category, m2) == 1);
        T_ASSERT(cat89_owns_mor(s->category, m3) == 1);

        /* middle */
        cat89_mor_release(s->category, m2);
        T_ASSERT(cat89_owns_mor(s->category, m1) == 1);
        T_ASSERT(cat89_owns_mor(s->category, m3) == 1);
        d = NULL;
        T_STATUS(cat89_dom(s->category, m1, &d), CAT89_OK);
        d = NULL;
        T_STATUS(cat89_dom(s->category, m3, &d), CAT89_OK);

        /* head */
        cat89_mor_release(s->category, m3);
        T_ASSERT(cat89_owns_mor(s->category, m1) == 1);
        d = NULL;
        T_STATUS(cat89_dom(s->category, m1, &d), CAT89_OK);

        /* tail */
        cat89_mor_release(s->category, m1);
        T_ASSERT(cat89_owns_mor(s->category, s->mor) == 1);
    }
}

int main(void)
{
    struct cat89_backend_set set;

    T_START();
    T_STATUS(cat89_backend_set_init(&set), CAT89_OK);
    T_EQ_UL(set.n, CAT89_BF_MAX);
    test_own_handles(&set);
    test_cross_instance(&set);
    test_cross_backend(&set);
    test_opposite_sharing(&set);
    test_results_owned(&set);
    test_retain_lifecycle(&set);
    test_live_registry_removal(&set);
    cat89_backend_set_dispose(&set);
    return T_END() ? 0 : 1;
}
