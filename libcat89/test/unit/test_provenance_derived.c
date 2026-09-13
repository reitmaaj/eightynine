/* cat89_test_provenance_derived.c - derived constructors and accessors.
 *
 * Two independent instances of each derived backend are used: components of
 * instance B must be rejected by instance A before any cast/retain/map, and
 * every raw accessor returns NULL for foreign or NULL handles. */
#include <backend_fixtures.h>
#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static struct cat89_bf_slot *slot_of(struct cat89_backend_set *set,
                                     enum cat89_bf_kind kind,
                                     unsigned long inst)
{
    return &set->slots[(unsigned long)kind * 2 + inst];
}

/* ------------------------------------------------- DC: product */

static void test_dc_product(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *p1 = slot_of(set, CAT89_BF_PRODUCT, 0);
    struct cat89_bf_slot *p2 = slot_of(set, CAT89_BF_PRODUCT, 1);
    const cat89_obj *foreign_l;
    const cat89_obj *foreign_r;
    const cat89_mor *foreign_lm;
    const cat89_mor *foreign_rm;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;
    cat89_status st;

    foreign_l = cat89_product_left_obj(p2->category, p2->obj);
    foreign_r = cat89_product_right_obj(p2->category, p2->obj);
    foreign_lm = cat89_product_left_mor(p2->category, p2->mor);
    foreign_rm = cat89_product_right_mor(p2->category, p2->mor);
    T_ASSERT(foreign_l && foreign_r && foreign_lm && foreign_rm);

    out_obj = (const cat89_obj *)(const void *)&out_obj;
    st = cat89_product_obj_new(p1->category, foreign_l,
                               cat89_product_right_obj(p1->category, p1->obj),
                               &out_obj);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    st = cat89_product_obj_new(p1->category,
                               cat89_product_left_obj(p1->category, p1->obj),
                               foreign_r, &out_obj);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(out_obj == NULL);

    out_mor = (cat89_mor *)(void *)&out_mor;
    st = cat89_product_mor_new(
        p1->category, (cat89_mor *)foreign_lm,
        (cat89_mor *)cat89_product_right_mor(p1->category, p1->mor), &out_mor);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    st = cat89_product_mor_new(
        p1->category,
        (cat89_mor *)cat89_product_left_mor(p1->category, p1->mor),
        (cat89_mor *)foreign_rm, &out_mor);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
}

/* --------------------------------------------------- DC: comma */

static void test_dc_comma(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *c1 = slot_of(set, CAT89_BF_COMMA, 0);
    struct cat89_bf_slot *c2 = slot_of(set, CAT89_BF_COMMA, 1);
    const cat89_obj *fc;
    const cat89_obj *fd;
    const cat89_mor *ff;
    const cat89_mor *fu;
    const cat89_mor *fv;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;
    const cat89_obj *local_c;
    const cat89_obj *local_d;
    const cat89_mor *local_f;
    const cat89_mor *local_u;

    fc = cat89_comma_left_obj(c2->category, c2->obj);
    fd = cat89_comma_right_obj(c2->category, c2->obj);
    ff = cat89_comma_obj_mor(c2->category, c2->obj);
    fu = cat89_comma_left_mor(c2->category, c2->mor);
    fv = cat89_comma_right_mor(c2->category, c2->mor);
    local_c = cat89_comma_left_obj(c1->category, c1->obj);
    local_d = cat89_comma_right_obj(c1->category, c1->obj);
    local_f = cat89_comma_obj_mor(c1->category, c1->obj);
    local_u = cat89_comma_left_mor(c1->category, c1->mor);
    T_ASSERT(fc && fd && ff && fu && fv && local_c && local_d && local_f);
    T_ASSERT(local_u);

    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_comma_obj_new(c1->category, fc, (cat89_mor *)local_f,
                                 local_d, &out_obj),
             CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_comma_obj_new(c1->category, local_c, (cat89_mor *)local_f,
                                 fd, &out_obj),
             CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_comma_obj_new(c1->category, local_c, (cat89_mor *)ff,
                                 local_d, &out_obj),
             CAT89_INVALID);
    T_ASSERT(out_obj == NULL);

    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_comma_mor_new(c1->category, c2->obj, c1->obj,
                                 (cat89_mor *)local_u, (cat89_mor *)local_u,
                                 &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_comma_mor_new(c1->category, c1->obj, c2->obj,
                                 (cat89_mor *)local_u, (cat89_mor *)local_u,
                                 &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_comma_mor_new(c1->category, c1->obj, c1->obj,
                                 (cat89_mor *)fu, (cat89_mor *)local_u,
                                 &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_comma_mor_new(c1->category, c1->obj, c1->obj,
                                 (cat89_mor *)local_u, (cat89_mor *)fv,
                                 &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
}

/* --------------------------------------------------- DC: slice/coslice */

static void test_dc_slice(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *s1 = slot_of(set, CAT89_BF_SLICE, 0);
    struct cat89_bf_slot *s2 = slot_of(set, CAT89_BF_SLICE, 1);
    const cat89_mor *ff;
    const cat89_mor *fh;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;
    const cat89_mor *local_f;
    const cat89_mor *local_h;

    ff = cat89_slice_obj_mor(s2->category, s2->obj);
    fh = cat89_slice_mor_mor(s2->category, s2->mor);
    local_f = cat89_slice_obj_mor(s1->category, s1->obj);
    local_h = cat89_slice_mor_mor(s1->category, s1->mor);
    T_ASSERT(ff && fh && local_f && local_h);

    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_slice_obj_new(s1->category, (cat89_mor *)ff, &out_obj),
             CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_slice_mor_new(s1->category, s2->obj, s1->obj,
                                 (cat89_mor *)local_h, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_slice_mor_new(s1->category, s1->obj, s2->obj,
                                 (cat89_mor *)local_h, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_slice_mor_new(s1->category, s1->obj, s1->obj,
                                 (cat89_mor *)fh, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
}

static void test_dc_coslice(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *s1 = slot_of(set, CAT89_BF_COSLICE, 0);
    struct cat89_bf_slot *s2 = slot_of(set, CAT89_BF_COSLICE, 1);
    const cat89_mor *ff;
    const cat89_mor *fk;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;
    const cat89_mor *local_f;
    const cat89_mor *local_k;

    ff = cat89_coslice_obj_mor(s2->category, s2->obj);
    fk = cat89_coslice_mor_mor(s2->category, s2->mor);
    local_f = cat89_coslice_obj_mor(s1->category, s1->obj);
    local_k = cat89_coslice_mor_mor(s1->category, s1->mor);
    T_ASSERT(ff && fk && local_f && local_k);

    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_coslice_obj_new(s1->category, (cat89_mor *)ff, &out_obj),
             CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_coslice_mor_new(s1->category, s2->obj, s1->obj,
                                   (cat89_mor *)local_k, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_coslice_mor_new(s1->category, s1->obj, s2->obj,
                                   (cat89_mor *)local_k, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_coslice_mor_new(s1->category, s1->obj, s1->obj,
                                   (cat89_mor *)fk, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
}

/* --------------------------------------------------- DC: opposite/arrow */

static void test_dc_opposite(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *o1 = slot_of(set, CAT89_BF_OPPOSITE, 0);
    struct cat89_bf_slot *o2 = slot_of(set, CAT89_BF_OPPOSITE, 1);
    cat89_mor *out;

    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_opposite_mor(o1->category, o2->base1_id, &out),
             CAT89_INVALID);
    T_ASSERT(out == NULL);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_opposite_mor(o1->category, o2->mor, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
}

static void test_dc_arrow(void)
{
    cat89_category *b1;
    cat89_eq *e1;
    cat89_enum *n1;
    cat89_category *b2;
    cat89_eq *e2;
    cat89_enum *n2;
    cat89_category *a1;
    cat89_category *a2;
    const cat89_obj *bo1;
    const cat89_obj *bo2;
    cat89_mor *id1;
    cat89_mor *id2;
    const cat89_obj *o1;
    const cat89_obj *o2;
    cat89_mor *m1;
    cat89_mor *m2;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;

    b1 = NULL;
    e1 = NULL;
    n1 = NULL;
    b2 = NULL;
    e2 = NULL;
    n2 = NULL;
    a1 = NULL;
    a2 = NULL;
    bo1 = NULL;
    bo2 = NULL;
    id1 = NULL;
    id2 = NULL;
    o1 = NULL;
    o2 = NULL;
    m1 = NULL;
    m2 = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &b1, &e1, &n1), CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &b2, &e2, &n2), CAT89_OK);
    T_STATUS(cat89_arrow_category_new(b1, e1, NULL, &a1), CAT89_OK);
    T_STATUS(cat89_arrow_category_new(b2, e2, NULL, &a2), CAT89_OK);

    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(n1, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(n1, it, &bo1, &done), CAT89_OK);
        cat89_obj_iter_close(n1, it);
    }
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(n2, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(n2, it, &bo2, &done), CAT89_OK);
        cat89_obj_iter_close(n2, it);
    }
    T_STATUS(cat89_identity(b1, bo1, &id1), CAT89_OK);
    T_STATUS(cat89_identity(b2, bo2, &id2), CAT89_OK);
    T_STATUS(cat89_arrow_obj_new(a1, bo1, id1, bo1, &o1), CAT89_OK);
    T_STATUS(cat89_arrow_obj_new(a2, bo2, id2, bo2, &o2), CAT89_OK);
    T_STATUS(cat89_arrow_mor_new(a1, o1, o1, id1, id1, &m1), CAT89_OK);
    T_STATUS(cat89_arrow_mor_new(a2, o2, o2, id2, id2, &m2), CAT89_OK);

    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_arrow_obj_new(a1, bo2, id2, bo1, &out_obj), CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_arrow_obj_new(a1, bo1, id2, bo1, &out_obj), CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_arrow_obj_new(a1, bo1, id1, bo2, &out_obj), CAT89_INVALID);
    T_ASSERT(out_obj == NULL);

    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_arrow_mor_new(a1, o2, o1, id1, id1, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_arrow_mor_new(a1, o1, o2, id1, id1, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_arrow_mor_new(a1, o1, o1, id2, id1, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_arrow_mor_new(a1, o1, o1, id1, id2, &out_mor),
             CAT89_INVALID);
    T_ASSERT(out_mor == NULL);

    cat89_mor_release(a1, m1);
    cat89_mor_release(a2, m2);
    cat89_mor_release(b1, id1);
    cat89_mor_release(b2, id2);
    cat89_category_release(a1);
    cat89_category_release(a2);
    cat89_enum_release(n1);
    cat89_enum_release(n2);
    cat89_eq_release(e1);
    cat89_eq_release(e2);
    cat89_category_release(b1);
    cat89_category_release(b2);
}

/* ------------------------------------------------------- AC: accessors */

static void test_ac_objects(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *p1 = slot_of(set, CAT89_BF_PRODUCT, 0);
    struct cat89_bf_slot *p2 = slot_of(set, CAT89_BF_PRODUCT, 1);
    struct cat89_bf_slot *c1 = slot_of(set, CAT89_BF_COMMA, 0);
    struct cat89_bf_slot *c2 = slot_of(set, CAT89_BF_COMMA, 1);
    struct cat89_bf_slot *s1 = slot_of(set, CAT89_BF_SLICE, 0);
    struct cat89_bf_slot *s2 = slot_of(set, CAT89_BF_SLICE, 1);
    struct cat89_bf_slot *n1 = slot_of(set, CAT89_BF_COSLICE, 0);
    struct cat89_bf_slot *n2 = slot_of(set, CAT89_BF_COSLICE, 1);

    /* product */
    T_ASSERT(cat89_product_left_obj(p1->category, p1->obj) == p1->base1_obj);
    T_ASSERT(cat89_product_right_obj(p1->category, p1->obj) == p1->base2_obj);
    T_ASSERT(cat89_product_left_obj(p2->category, p1->obj) == NULL);
    T_ASSERT(cat89_product_left_obj(c1->category, p1->obj) == NULL);
    T_ASSERT(cat89_product_right_obj(p2->category, p1->obj) == NULL);
    T_ASSERT(cat89_product_left_obj(p1->category, NULL) == NULL);

    /* comma */
    T_ASSERT(cat89_comma_left_obj(c1->category, c1->obj) == c1->base1_obj);
    T_ASSERT(cat89_comma_right_obj(c1->category, c1->obj) == c1->base1_obj);
    T_ASSERT(cat89_comma_obj_mor(c1->category, c1->obj) == c1->base1_id);
    T_ASSERT(cat89_comma_left_obj(c2->category, c1->obj) == NULL);
    T_ASSERT(cat89_comma_left_obj(s1->category, c1->obj) == NULL);
    T_ASSERT(cat89_comma_right_obj(c2->category, c1->obj) == NULL);
    T_ASSERT(cat89_comma_obj_mor(c2->category, c1->obj) == NULL);
    T_ASSERT(cat89_comma_obj_mor(c1->category, NULL) == NULL);

    /* slice */
    T_ASSERT(cat89_slice_obj_mor(s1->category, s1->obj) == s1->base1_id);
    T_ASSERT(cat89_slice_obj_mor(s2->category, s1->obj) == NULL);
    T_ASSERT(cat89_slice_obj_mor(c1->category, s1->obj) == NULL);
    T_ASSERT(cat89_slice_obj_mor(s1->category, NULL) == NULL);

    /* coslice */
    T_ASSERT(cat89_coslice_obj_mor(n1->category, n1->obj) == n1->base1_id);
    T_ASSERT(cat89_coslice_obj_mor(n2->category, n1->obj) == NULL);
    T_ASSERT(cat89_coslice_obj_mor(s1->category, n1->obj) == NULL);
    T_ASSERT(cat89_coslice_obj_mor(n1->category, NULL) == NULL);
}

static void test_ac_morphisms(struct cat89_backend_set *set)
{
    struct cat89_bf_slot *p1 = slot_of(set, CAT89_BF_PRODUCT, 0);
    struct cat89_bf_slot *p2 = slot_of(set, CAT89_BF_PRODUCT, 1);
    struct cat89_bf_slot *c1 = slot_of(set, CAT89_BF_COMMA, 0);
    struct cat89_bf_slot *c2 = slot_of(set, CAT89_BF_COMMA, 1);
    struct cat89_bf_slot *s1 = slot_of(set, CAT89_BF_SLICE, 0);
    struct cat89_bf_slot *s2 = slot_of(set, CAT89_BF_SLICE, 1);
    struct cat89_bf_slot *n1 = slot_of(set, CAT89_BF_COSLICE, 0);
    struct cat89_bf_slot *n2 = slot_of(set, CAT89_BF_COSLICE, 1);
    struct cat89_bf_slot *o1 = slot_of(set, CAT89_BF_OPPOSITE, 0);
    struct cat89_bf_slot *o2 = slot_of(set, CAT89_BF_OPPOSITE, 1);

    /* product */
    T_ASSERT(cat89_product_left_mor(p1->category, p1->mor) == p1->base1_id);
    T_ASSERT(cat89_product_right_mor(p1->category, p1->mor) == p1->base2_id);
    T_ASSERT(cat89_product_left_mor(p2->category, p1->mor) == NULL);
    T_ASSERT(cat89_product_left_mor(c1->category, p1->mor) == NULL);
    T_ASSERT(cat89_product_right_mor(p2->category, p1->mor) == NULL);
    T_ASSERT(cat89_product_left_mor(p1->category, NULL) == NULL);

    /* comma */
    T_ASSERT(cat89_comma_left_mor(c1->category, c1->mor) == c1->base1_id);
    T_ASSERT(cat89_comma_right_mor(c1->category, c1->mor) == c1->base1_id);
    T_ASSERT(cat89_comma_left_mor(c2->category, c1->mor) == NULL);
    T_ASSERT(cat89_comma_left_mor(s1->category, c1->mor) == NULL);
    T_ASSERT(cat89_comma_right_mor(c2->category, c1->mor) == NULL);
    T_ASSERT(cat89_comma_left_mor(c1->category, NULL) == NULL);

    /* slice */
    T_ASSERT(cat89_slice_mor_mor(s1->category, s1->mor) == s1->base1_id);
    T_ASSERT(cat89_slice_mor_mor(s2->category, s1->mor) == NULL);
    T_ASSERT(cat89_slice_mor_mor(c1->category, s1->mor) == NULL);
    T_ASSERT(cat89_slice_mor_mor(s1->category, NULL) == NULL);

    /* coslice */
    T_ASSERT(cat89_coslice_mor_mor(n1->category, n1->mor) == n1->base1_id);
    T_ASSERT(cat89_coslice_mor_mor(n2->category, n1->mor) == NULL);
    T_ASSERT(cat89_coslice_mor_mor(s1->category, n1->mor) == NULL);
    T_ASSERT(cat89_coslice_mor_mor(n1->category, NULL) == NULL);

    /* opposite */
    T_ASSERT(cat89_opposite_base_mor(o1->category, o1->mor) == o1->base1_id);
    T_ASSERT(cat89_opposite_base_mor(o2->category, o1->mor) == NULL);
    T_ASSERT(cat89_opposite_base_mor(p1->category, o1->mor) == NULL);
    T_ASSERT(cat89_opposite_base_mor(o1->category, NULL) == NULL);
}

int main(void)
{
    struct cat89_backend_set set;

    T_START();
    T_STATUS(cat89_backend_set_init(&set), CAT89_OK);
    test_dc_product(&set);
    test_dc_comma(&set);
    test_dc_slice(&set);
    test_dc_coslice(&set);
    test_dc_opposite(&set);
    test_dc_arrow();
    test_ac_objects(&set);
    test_ac_morphisms(&set);
    cat89_backend_set_dispose(&set);
    return T_END() ? 0 : 1;
}
