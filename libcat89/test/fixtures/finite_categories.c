/* cat89_finite_fixtures.c - reference finite category fixtures. */

#include "finite_categories.h"
#include <cat89_internal.h>

static cat89_status build_terminal(cat89_category **cat, cat89_eq **eq,
                                   cat89_enum **enumeration)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o = 0;
    cat89_finite_mor_id id = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &o);
    cat89_finite_add_morphism(b, o, o, &id);
    cat89_finite_set_identity(b, o, id);
    cat89_finite_set_composition(b, id, id, id);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

static cat89_status build_discrete2(cat89_category **cat, cat89_eq **eq,
                                    cat89_enum **enumeration)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_obj_id c = 0;
    cat89_finite_mor_id ia = 0;
    cat89_finite_mor_id ic = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &c);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, c, c, &ic);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, c, ic);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ic, ic, ic);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

static cat89_status build_arrow(cat89_category **cat, cat89_eq **eq,
                                cat89_enum **enumeration)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_obj_id c = 0;
    cat89_finite_mor_id ia = 0;
    cat89_finite_mor_id ic = 0;
    cat89_finite_mor_id f = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &c);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, c, c, &ic);
    cat89_finite_add_morphism(b, a, c, &f);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, c, ic);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ic, ic, ic);
    cat89_finite_set_composition(b, f, ia, f);
    cat89_finite_set_composition(b, ic, f, f);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

static cat89_status build_comp2(cat89_category **cat, cat89_eq **eq,
                                cat89_enum **enumeration)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_obj_id c = 0;
    cat89_finite_obj_id d = 0;
    cat89_finite_mor_id ia = 0;
    cat89_finite_mor_id ib = 0;
    cat89_finite_mor_id ic = 0;
    cat89_finite_mor_id f = 0;
    cat89_finite_mor_id g = 0;
    cat89_finite_mor_id h = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &c);
    cat89_finite_add_object(b, &d);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, c, c, &ib);
    cat89_finite_add_morphism(b, d, d, &ic);
    cat89_finite_add_morphism(b, a, c, &f);
    cat89_finite_add_morphism(b, c, d, &g);
    cat89_finite_add_morphism(b, a, d, &h);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, c, ib);
    cat89_finite_set_identity(b, d, ic);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ib, ib, ib);
    cat89_finite_set_composition(b, ic, ic, ic);
    cat89_finite_set_composition(b, f, ia, f);
    cat89_finite_set_composition(b, ib, f, f);
    cat89_finite_set_composition(b, g, ib, g);
    cat89_finite_set_composition(b, ic, g, g);
    cat89_finite_set_composition(b, g, f, h);
    cat89_finite_set_composition(b, ic, h, h);
    cat89_finite_set_composition(b, h, ia, h);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* Add every identity-cancellation composition row so the built category is
 * lawful under the finder's compositions: id(cod(m)) o m == m and
 * m o id(dom(m)) == m. Not green-owned (fixture code). */
static void add_identity_rows(cat89_finite_builder *b, const unsigned long *dom,
                              const unsigned long *cod,
                              const unsigned long *ident, unsigned long nmor)
{
    unsigned long i;

    for (i = 0; i < nmor; i = i + 1)
    {
        cat89_finite_set_composition(b, ident[cod[i]], i, i);
        cat89_finite_set_composition(b, i, ident[dom[i]], i);
    }
}

/* CAT89_FIX_BINPROD: objects A,B,P,X; P = A x B with projections pA,pB and a
 * separate cone object X (xA,xB,m) so the finder must not mistake X for the
 * apex. Morphism ids:
 *  0 idA  1 idB  2 idP  3 idX  4 pA:P->A  5 pB:P->B  6 xA:X->A
 *  7 xB:X->B  8 m:X->P   (pA o m = xA, pB o m = xB). */
static cat89_status build_binprod(cat89_category **cat, cat89_eq **eq,
                                  cat89_enum **enumeration)
{
    const unsigned long A = 0;
    const unsigned long B = 1;
    const unsigned long P = 2;
    const unsigned long X = 3;
    unsigned long dom[9];
    unsigned long cod[9];
    unsigned long ident[4];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 9; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);

    mid = 0;
    cat89_finite_add_morphism(b, A, A, &mid);
    dom[0] = A;
    cod[0] = A;
    ident[A] = 0;
    cat89_finite_add_morphism(b, B, B, &mid);
    dom[1] = B;
    cod[1] = B;
    ident[B] = 1;
    cat89_finite_add_morphism(b, P, P, &mid);
    dom[2] = P;
    cod[2] = P;
    ident[P] = 2;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[3] = X;
    cod[3] = X;
    ident[X] = 3;
    cat89_finite_add_morphism(b, P, A, &mid);
    dom[4] = P;
    cod[4] = A;
    cat89_finite_add_morphism(b, P, B, &mid);
    dom[5] = P;
    cod[5] = B;
    cat89_finite_add_morphism(b, X, A, &mid);
    dom[6] = X;
    cod[6] = A;
    cat89_finite_add_morphism(b, X, B, &mid);
    dom[7] = X;
    cod[7] = B;
    cat89_finite_add_morphism(b, X, P, &mid);
    dom[8] = X;
    cod[8] = P;

    cat89_finite_set_identity(b, A, ident[A]);
    cat89_finite_set_identity(b, B, ident[B]);
    cat89_finite_set_identity(b, P, ident[P]);
    cat89_finite_set_identity(b, X, ident[X]);
    add_identity_rows(b, dom, cod, ident, 9);
    cat89_finite_set_composition(b, 4, 8, 6);
    cat89_finite_set_composition(b, 5, 8, 7);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* CAT89_FIX_BINCOPROD: objects A,B,S,X; S = A + B with injections iA,iB and a
 * separate cocone object X (yA,yB,d) so the finder must not mistake X for the
 * apex. Morphism ids:
 *  0 idA  1 idB  2 idS  3 idX  4 iA:A->S  5 iB:B->S  6 yA:A->X
 *  7 yB:B->X  8 d:S->X   (d o iA = yA, d o iB = yB). */
static cat89_status build_bincoprod(cat89_category **cat, cat89_eq **eq,
                                    cat89_enum **enumeration)
{
    const unsigned long A = 0;
    const unsigned long B = 1;
    const unsigned long S = 2;
    const unsigned long X = 3;
    unsigned long dom[9];
    unsigned long cod[9];
    unsigned long ident[4];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 9; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);

    mid = 0;
    cat89_finite_add_morphism(b, A, A, &mid);
    dom[0] = A;
    cod[0] = A;
    ident[A] = 0;
    cat89_finite_add_morphism(b, B, B, &mid);
    dom[1] = B;
    cod[1] = B;
    ident[B] = 1;
    cat89_finite_add_morphism(b, S, S, &mid);
    dom[2] = S;
    cod[2] = S;
    ident[S] = 2;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[3] = X;
    cod[3] = X;
    ident[X] = 3;
    cat89_finite_add_morphism(b, A, S, &mid);
    dom[4] = A;
    cod[4] = S;
    cat89_finite_add_morphism(b, B, S, &mid);
    dom[5] = B;
    cod[5] = S;
    cat89_finite_add_morphism(b, A, X, &mid);
    dom[6] = A;
    cod[6] = X;
    cat89_finite_add_morphism(b, B, X, &mid);
    dom[7] = B;
    cod[7] = X;
    cat89_finite_add_morphism(b, S, X, &mid);
    dom[8] = S;
    cod[8] = X;

    cat89_finite_set_identity(b, A, ident[A]);
    cat89_finite_set_identity(b, B, ident[B]);
    cat89_finite_set_identity(b, S, ident[S]);
    cat89_finite_set_identity(b, X, ident[X]);
    add_identity_rows(b, dom, cod, ident, 9);
    cat89_finite_set_composition(b, 8, 4, 6);
    cat89_finite_set_composition(b, 8, 5, 7);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* CAT89_FIX_EQUALIZER: objects E,X,Y,Z with parallel f,g:X->Y; e:E->X is their
 * equalizer (f o e == g o e), Z is a decoy equalizing object that is NOT the
 * apex. Morphism ids:
 *  0 idE 1 idX 2 idY 3 idZ 4 e:E->X 5 f:X->Y 6 g:X->Y 7 z:Z->X 8 d:Z->E
 *  9 y:E->Y (f o e = g o e = y) 10 mz:Z->Y (f o z = g o z = mz)
 *  f o e = y, g o e = y, f o z = mz, g o z = mz, e o d = z. */
static cat89_status build_equalizer(cat89_category **cat, cat89_eq **eq,
                                    cat89_enum **enumeration)
{
    const unsigned long E = 0;
    const unsigned long X = 1;
    const unsigned long Y = 2;
    const unsigned long Z = 3;
    unsigned long dom[11];
    unsigned long cod[11];
    unsigned long ident[4];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 11; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    mid = 0;
    cat89_finite_add_morphism(b, E, E, &mid);
    dom[0] = E;
    cod[0] = E;
    ident[E] = 0;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[1] = X;
    cod[1] = X;
    ident[X] = 1;
    cat89_finite_add_morphism(b, Y, Y, &mid);
    dom[2] = Y;
    cod[2] = Y;
    ident[Y] = 2;
    cat89_finite_add_morphism(b, Z, Z, &mid);
    dom[3] = Z;
    cod[3] = Z;
    ident[Z] = 3;
    cat89_finite_add_morphism(b, E, X, &mid);
    dom[4] = E;
    cod[4] = X;
    cat89_finite_add_morphism(b, X, Y, &mid);
    dom[5] = X;
    cod[5] = Y;
    cat89_finite_add_morphism(b, X, Y, &mid);
    dom[6] = X;
    cod[6] = Y;
    cat89_finite_add_morphism(b, Z, X, &mid);
    dom[7] = Z;
    cod[7] = X;
    cat89_finite_add_morphism(b, Z, E, &mid);
    dom[8] = Z;
    cod[8] = E;
    cat89_finite_add_morphism(b, E, Y, &mid);
    dom[9] = E;
    cod[9] = Y;
    cat89_finite_add_morphism(b, Z, Y, &mid);
    dom[10] = Z;
    cod[10] = Y;

    cat89_finite_set_identity(b, E, ident[E]);
    cat89_finite_set_identity(b, X, ident[X]);
    cat89_finite_set_identity(b, Y, ident[Y]);
    cat89_finite_set_identity(b, Z, ident[Z]);
    add_identity_rows(b, dom, cod, ident, 11);
    cat89_finite_set_composition(b, 5, 4, 9);
    cat89_finite_set_composition(b, 6, 4, 9);
    cat89_finite_set_composition(b, 5, 7, 10);
    cat89_finite_set_composition(b, 6, 7, 10);
    cat89_finite_set_composition(b, 4, 8, 7);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* CAT89_FIX_COEQUALIZER: objects Q,X,Y,W with parallel f,g:X->Y; c:Y->Q is
 * their coequalizer (c o f == c o g == k); W is a decoy coequalizing object
 * (a1 o f == a1 o g == a0) that is NOT the apex. Morphism ids:
 *  0 idQ 1 idX 2 idY 3 idW 4 f:X->Y 5 g:X->Y 6 c:Y->Q 7 k:X->Q
 *  8 a1:Y->W 9 a0:X->W 10 d:Q->W
 *  c o f = k, c o g = k, a1 o f = a0, a1 o g = a0, d o c = a1. */
static cat89_status build_coequalizer(cat89_category **cat, cat89_eq **eq,
                                      cat89_enum **enumeration)
{
    const unsigned long Q = 0;
    const unsigned long X = 1;
    const unsigned long Y = 2;
    const unsigned long W = 3;
    unsigned long dom[11];
    unsigned long cod[11];
    unsigned long ident[4];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 11; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    mid = 0;
    cat89_finite_add_morphism(b, Q, Q, &mid);
    dom[0] = Q;
    cod[0] = Q;
    ident[Q] = 0;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[1] = X;
    cod[1] = X;
    ident[X] = 1;
    cat89_finite_add_morphism(b, Y, Y, &mid);
    dom[2] = Y;
    cod[2] = Y;
    ident[Y] = 2;
    cat89_finite_add_morphism(b, W, W, &mid);
    dom[3] = W;
    cod[3] = W;
    ident[W] = 3;
    cat89_finite_add_morphism(b, X, Y, &mid);
    dom[4] = X;
    cod[4] = Y;
    cat89_finite_add_morphism(b, X, Y, &mid);
    dom[5] = X;
    cod[5] = Y;
    cat89_finite_add_morphism(b, Y, Q, &mid);
    dom[6] = Y;
    cod[6] = Q;
    cat89_finite_add_morphism(b, X, Q, &mid);
    dom[7] = X;
    cod[7] = Q;
    cat89_finite_add_morphism(b, Y, W, &mid);
    dom[8] = Y;
    cod[8] = W;
    cat89_finite_add_morphism(b, X, W, &mid);
    dom[9] = X;
    cod[9] = W;
    cat89_finite_add_morphism(b, Q, W, &mid);
    dom[10] = Q;
    cod[10] = W;

    cat89_finite_set_identity(b, Q, ident[Q]);
    cat89_finite_set_identity(b, X, ident[X]);
    cat89_finite_set_identity(b, Y, ident[Y]);
    cat89_finite_set_identity(b, W, ident[W]);
    add_identity_rows(b, dom, cod, ident, 11);
    cat89_finite_set_composition(b, 6, 4, 7);
    cat89_finite_set_composition(b, 6, 5, 7);
    cat89_finite_set_composition(b, 8, 4, 9);
    cat89_finite_set_composition(b, 8, 5, 9);
    cat89_finite_set_composition(b, 10, 6, 8);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* CAT89_FIX_PULLBACK: objects P,X,Y,Z,W with f:X->Z, g:Y->Z; P is their
 * pullback (pX:P->X, pY:P->Y, f o pX == g o pY == zl); W is a decoy commuting
 * object (wX,wY) that is NOT the apex. Morphism ids:
 *  0 idP 1 idX 2 idY 3 idZ 4 idW 5 pX:P->X 6 pY:P->Y 7 zl:P->Z 8 f:X->Z
 *  9 g:Y->Z 10 wX:W->X 11 wY:W->Y 12 zW:W->Z 13 d:W->P
 *  f o pX=zl, g o pY=zl, f o wX=zW, g o wY=zW, pX o d=wX, pY o d=wY. */
static cat89_status build_pullback(cat89_category **cat, cat89_eq **eq,
                                   cat89_enum **enumeration)
{
    const unsigned long P = 0;
    const unsigned long X = 1;
    const unsigned long Y = 2;
    const unsigned long Z = 3;
    const unsigned long W = 4;
    unsigned long dom[14];
    unsigned long cod[14];
    unsigned long ident[5];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 14; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    mid = 0;
    cat89_finite_add_morphism(b, P, P, &mid);
    dom[0] = P;
    cod[0] = P;
    ident[P] = 0;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[1] = X;
    cod[1] = X;
    ident[X] = 1;
    cat89_finite_add_morphism(b, Y, Y, &mid);
    dom[2] = Y;
    cod[2] = Y;
    ident[Y] = 2;
    cat89_finite_add_morphism(b, Z, Z, &mid);
    dom[3] = Z;
    cod[3] = Z;
    ident[Z] = 3;
    cat89_finite_add_morphism(b, W, W, &mid);
    dom[4] = W;
    cod[4] = W;
    ident[W] = 4;
    cat89_finite_add_morphism(b, P, X, &mid);
    dom[5] = P;
    cod[5] = X;
    cat89_finite_add_morphism(b, P, Y, &mid);
    dom[6] = P;
    cod[6] = Y;
    cat89_finite_add_morphism(b, P, Z, &mid);
    dom[7] = P;
    cod[7] = Z;
    cat89_finite_add_morphism(b, X, Z, &mid);
    dom[8] = X;
    cod[8] = Z;
    cat89_finite_add_morphism(b, Y, Z, &mid);
    dom[9] = Y;
    cod[9] = Z;
    cat89_finite_add_morphism(b, W, X, &mid);
    dom[10] = W;
    cod[10] = X;
    cat89_finite_add_morphism(b, W, Y, &mid);
    dom[11] = W;
    cod[11] = Y;
    cat89_finite_add_morphism(b, W, Z, &mid);
    dom[12] = W;
    cod[12] = Z;
    cat89_finite_add_morphism(b, W, P, &mid);
    dom[13] = W;
    cod[13] = P;

    cat89_finite_set_identity(b, P, ident[P]);
    cat89_finite_set_identity(b, X, ident[X]);
    cat89_finite_set_identity(b, Y, ident[Y]);
    cat89_finite_set_identity(b, Z, ident[Z]);
    cat89_finite_set_identity(b, W, ident[W]);
    add_identity_rows(b, dom, cod, ident, 14);
    cat89_finite_set_composition(b, 8, 5, 7);
    cat89_finite_set_composition(b, 9, 6, 7);
    cat89_finite_set_composition(b, 8, 10, 12);
    cat89_finite_set_composition(b, 9, 11, 12);
    cat89_finite_set_composition(b, 5, 13, 10);
    cat89_finite_set_composition(b, 6, 13, 11);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

/* CAT89_FIX_PUSHOUT: objects Q,X,Y,Z,W with f:X->Y, g:X->Z; Q is their pushout
 * (iY:Y->Q, iZ:Z->Q, iY o f == iZ o g == kX); W is a decoy cocone object that
 * is NOT the apex. Morphism ids:
 *  0 idQ 1 idX 2 idY 3 idZ 4 idW 5 f:X->Y 6 g:X->Z 7 iY:Y->Q 8 iZ:Z->Q
 *  9 kX:X->Q (iY o f == iZ o g == kX) 10 aY:Y->W 11 aZ:Z->W 12 aX:X->W
 *  13 e:Q->W  (iY o f = kX, iZ o g = kX, aY o f = aX, aZ o g = aX, e o iY = aY,
 *              e o iZ = aZ). */
static cat89_status build_pushout(cat89_category **cat, cat89_eq **eq,
                                  cat89_enum **enumeration)
{
    const unsigned long Q = 0;
    const unsigned long X = 1;
    const unsigned long Y = 2;
    const unsigned long Z = 3;
    const unsigned long W = 4;
    unsigned long dom[14];
    unsigned long cod[14];
    unsigned long ident[5];
    cat89_finite_builder *b;
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < 14; i = i + 1)
    {
        dom[i] = 0;
        cod[i] = 0;
    }
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    cat89_finite_add_object(b, &oid);
    mid = 0;
    cat89_finite_add_morphism(b, Q, Q, &mid);
    dom[0] = Q;
    cod[0] = Q;
    ident[Q] = 0;
    cat89_finite_add_morphism(b, X, X, &mid);
    dom[1] = X;
    cod[1] = X;
    ident[X] = 1;
    cat89_finite_add_morphism(b, Y, Y, &mid);
    dom[2] = Y;
    cod[2] = Y;
    ident[Y] = 2;
    cat89_finite_add_morphism(b, Z, Z, &mid);
    dom[3] = Z;
    cod[3] = Z;
    ident[Z] = 3;
    cat89_finite_add_morphism(b, W, W, &mid);
    dom[4] = W;
    cod[4] = W;
    ident[W] = 4;
    cat89_finite_add_morphism(b, X, Y, &mid);
    dom[5] = X;
    cod[5] = Y;
    cat89_finite_add_morphism(b, X, Z, &mid);
    dom[6] = X;
    cod[6] = Z;
    cat89_finite_add_morphism(b, Y, Q, &mid);
    dom[7] = Y;
    cod[7] = Q;
    cat89_finite_add_morphism(b, Z, Q, &mid);
    dom[8] = Z;
    cod[8] = Q;
    cat89_finite_add_morphism(b, X, Q, &mid);
    dom[9] = X;
    cod[9] = Q;
    cat89_finite_add_morphism(b, Y, W, &mid);
    dom[10] = Y;
    cod[10] = W;
    cat89_finite_add_morphism(b, Z, W, &mid);
    dom[11] = Z;
    cod[11] = W;
    cat89_finite_add_morphism(b, X, W, &mid);
    dom[12] = X;
    cod[12] = W;
    cat89_finite_add_morphism(b, Q, W, &mid);
    dom[13] = Q;
    cod[13] = W;

    cat89_finite_set_identity(b, Q, ident[Q]);
    cat89_finite_set_identity(b, X, ident[X]);
    cat89_finite_set_identity(b, Y, ident[Y]);
    cat89_finite_set_identity(b, Z, ident[Z]);
    cat89_finite_set_identity(b, W, ident[W]);
    add_identity_rows(b, dom, cod, ident, 14);
    cat89_finite_set_composition(b, 7, 5, 9);
    cat89_finite_set_composition(b, 8, 6, 9);
    cat89_finite_set_composition(b, 10, 5, 12);
    cat89_finite_set_composition(b, 11, 6, 12);
    cat89_finite_set_composition(b, 13, 7, 10);
    cat89_finite_set_composition(b, 13, 8, 11);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

static cat89_status build_c2(cat89_category **cat, cat89_eq **eq,
                             cat89_enum **enumeration)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o = 0;
    cat89_finite_mor_id e = 0;
    cat89_finite_mor_id s = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &o);
    cat89_finite_add_morphism(b, o, o, &e);
    cat89_finite_add_morphism(b, o, o, &s);
    cat89_finite_set_identity(b, o, e);
    cat89_finite_set_composition(b, e, e, e);
    cat89_finite_set_composition(b, e, s, s);
    cat89_finite_set_composition(b, s, e, s);
    cat89_finite_set_composition(b, s, s, e);
    st = cat89_finite_build_unchecked(b, cat, eq, enumeration);
    cat89_finite_builder_release(b);
    return st;
}

cat89_status cat89_fixture_build(enum cat89_fixture_kind kind,
                                 cat89_category **out_category,
                                 cat89_eq **out_eq, cat89_enum **out_enum)
{
    switch (kind)
    {
    case CAT89_FIX_TERMINAL:
        return build_terminal(out_category, out_eq, out_enum);
    case CAT89_FIX_DISCRETE2:
        return build_discrete2(out_category, out_eq, out_enum);
    case CAT89_FIX_ARROW:
        return build_arrow(out_category, out_eq, out_enum);
    case CAT89_FIX_COMP2:
        return build_comp2(out_category, out_eq, out_enum);
    case CAT89_FIX_C2_GROUPOID:
        return build_c2(out_category, out_eq, out_enum);
    case CAT89_FIX_BINPROD:
        return build_binprod(out_category, out_eq, out_enum);
    case CAT89_FIX_BINCOPROD:
        return build_bincoprod(out_category, out_eq, out_enum);
    case CAT89_FIX_EQUALIZER:
        return build_equalizer(out_category, out_eq, out_enum);
    case CAT89_FIX_COEQUALIZER:
        return build_coequalizer(out_category, out_eq, out_enum);
    case CAT89_FIX_PULLBACK:
        return build_pullback(out_category, out_eq, out_enum);
    case CAT89_FIX_PUSHOUT:
        return build_pushout(out_category, out_eq, out_enum);
    default:
        return CAT89_INVALID;
    }
}
