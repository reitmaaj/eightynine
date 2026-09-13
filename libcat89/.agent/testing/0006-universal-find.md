# libcat89 testing scenarios (BDD) - W4.2 finite universal-construction finders

## 0006-universal-find.md

Universal constructions (binary product/coproduct, equalizer/coequalizer,
pullback/pushout) are detected over a finite ambient category by an explicit
enumeration + cardinality/uniqueness test, and are returned as a `cat89_limit`
or `cat89_colimit` whose limiting cone carries a real universal `factor`
(CAT-I9). Detection needs decidable morphism equality, so every finder takes an
explicit `cat89_eq`; an unavailable required capability is never silently
assumed.

- SCENARIO binary product found: GIVEN a finite ambient category C containing a
  binary product P = a x b with projections p1 : P -> a and p2 : P -> b WHEN the
  binary-product finder runs for objects a, b THEN it returns CAT89_OK and a
  cat89_limit whose cone apex is P, whose legs are p1 and p2, and whose factor
  maps every candidate cone (X, x1 : X -> a, x2 : X -> b) to the unique
  u : X -> P with p1 o u == x1 and p2 o u == x2.
- SCENARIO binary product universality: GIVEN the returned product limit and a
  candidate cone over a == a and b == b with apex X and legs x1, x2 WHEN the
  limit's factor runs THEN it returns the unique mediating morphism u and
  composing p1 o u / p2 o u reproduces x1 / x2.
- SCENARIO binary product absent: GIVEN a finite ambient category with no
  binary product of a and b WHEN the finder runs THEN it returns
  CAT89_NOT_FOUND and leaves *out_limit == NULL (no silent bogus limit).
- SCENARIO finder rejects unusable input: GIVEN NULL category/enumeration/eq/
  object/out-parameter WHEN the finder runs THEN it returns CAT89_INVALID and
  leaves *out_limit == NULL.
- SCENARIO coproduct found (dual): GIVEN a finite ambient category C containing
  a binary coproduct with injections i1 : a -> S, i2 : b -> S WHEN the
  binary-coproduct finder runs for a, b THEN it returns a cat89_colimit whose
  cocone apex is S, whose legs are the injections, and whose factor maps every
  candidate cocone (X, x1 : a -> X, x2 : b -> X) to the unique
  u : S -> X with u o i1 == x1 and u o i2 == x2.
- SCENARIO equalizer found: GIVEN a finite ambient category C with two parallel
  arrows f, g : X -> Y and an equalizer e : E -> X (so f o e == g o e, universal)
  WHEN the equalizer finder runs for f, g THEN it returns a cat89_limit of the
  PARALLEL diagram whose cone apex is E, whose leg into X is e, and whose factor
  maps every candidate cone (Z, z : Z -> X with f o z == g o z) to the unique
  u : Z -> E with e o u == z.
- SCENARIO equalizer decoy not substituted: GIVEN an object Z carrying an
  equalizing arrow z : Z -> X that is not universal (no mediating arrow to the
  true apex E for every equalizing cone) WHEN the equalizer finder runs THEN it
  does not return Z; it returns the genuine equalizer apex E.
- SCENARIO coequalizer found (dual): GIVEN two parallel arrows f, g : X -> Y and
  a coequalizer c : Y -> Q (so c o f == c o g, universal) WHEN the coequalizer
  finder runs for f, g THEN it returns a cat89_colimit whose cocone leg into Q
  is c and whose factor maps every candidate cocone to the unique mediating
  arrow Q -> A.
- SCENARIO equalizer absent: GIVEN two parallel arrows with no equalizer WHEN
  the finder runs THEN it returns CAT89_NOT_FOUND and *out == NULL.
- SCENARIO pullback found: GIVEN a finite ambient category C with arrows
  f : X -> Z, g : Y -> Z and a pullback P (legs pX : P -> X, pY : P -> Y,
  f o pX == g o pY, universal) WHEN the pullback finder runs for f, g THEN it
  returns a cat89_limit of the SPAN diagram whose cone apex is P and whose
  factor maps every candidate cone (W, wX : W -> X, wY : W -> Y with
  f o wX == g o wY) to the unique u : W -> P with pX o u == wX and pY o u == wY.
- SCENARIO pullback decoy not substituted: GIVEN an object W with a commuting
  pair into X, Y that is not universal WHEN the pullback finder runs THEN it
  does not return W; it returns the genuine pullback apex P.
- SCENARIO pushout found (dual): GIVEN arrows f : X -> Y, g : X -> Z and a
  pushout Q WHEN the pushout finder runs THEN it returns a cat89_colimit of the
  COSPAN diagram with a working universal factor.
- SCENARIO pullback absent: GIVEN two arrows with equal codomain and no pullback
  WHEN the finder runs THEN it returns CAT89_NOT_FOUND and *out == NULL.
- SCENARIO product target apex is not wrongly substituted: GIVEN an ambient
  where an object X carries arrows to a and b but is not a product (lacks the
  universal mediating arrow to the true apex) WHEN the finder runs THEN it does
  not return X; it returns the genuine product apex P.
- SCENARIO wrapper/recursive composition: GIVEN a product of composites whose
  underlying ambient is itself built from finite composites WHEN the finder and
  its factor run THEN the universal equations hold (foundations for later
  equalizer/pullback reuse of the same engine).
