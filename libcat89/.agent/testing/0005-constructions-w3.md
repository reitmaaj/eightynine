# libcat89 testing scenarios (BDD) - W3 generic wrapper categories

## 0005-constructions-w3.md

- SCENARIO product dom/cod/identity: GIVEN a product category C x D and product
  object (A,B) WHEN identity((A,B)) runs THEN the identity has dom=(A,B) and
  cod=(A,B) and its components are the identities of A and B.
- SCENARIO product composition componentwise: GIVEN composable product
  morphisms (f2,g2):(A2,B2)->(A3,B3) and (f1,g1):(A1,B1)->(A2,B2) WHEN
  composing THEN (f2,g2) o (f1,g1) = (f2 o f1, g2 o g1) with dom=(A1,B1) and
  cod=(A3,B3).
- SCENARIO product ill-typed rejected: GIVEN product morphisms that are not
  componentwise composable WHEN cat89_compose runs THEN CAT89_DOMAIN and the
  backend compose is not invoked.
- SCENARIO product distinct-equal objects: GIVEN two product objects that are
  componentwise equal but built separately WHEN obj_same runs THEN it returns
  true while the object handles are not pointer-identical (CAT-I10).
- SCENARIO wrapper categories obey laws: GIVEN product, comma, slice/coslice,
  arrow over finite bases WHEN identity/associativity are checked exhaustively
  via test-supplied equality/enumeration THEN all cases pass (ICD-I10).
- SCENARIO slice triangle: GIVEN slice objects f:X->A and g:Y->A in C/A WHEN
  h:X->Y is a slice morphism THEN g o h = f is required; a non-commuting h is
  invalid under the checker.
- SCENARIO comma square: GIVEN comma category F↓G objects (a,f,b) with
  f:F(a)->G(b) WHEN a morphism (u,v):(a,f,b)->(a',f',b') runs THEN
  G(v) o f = f' o F(u) is required.
- SCENARIO arrow commutative square: GIVEN arrow-category objects that are
  morphisms of C WHEN an arrow morphism (u,v):f->g runs THEN v o f = g o u.
- SCENARIO recursive wrapper: GIVEN (C x D) x E or slice/arrow inside a product
  WHEN the exhaustive law check runs with nested wrapper equality/enumeration
  THEN all cases pass.
