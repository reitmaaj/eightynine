# libcat89 testing scenarios (BDD) - W5 exhaustive law families (leftover)

## 0007-exhaustive-families-w5.md

The exhaustive category-law checker landed as `cat89_check_category_exhaustive`
(identity + associativity) over an enumerated finite ambient. This file records
the REMAINING W5 scope: exhaustively verifying the derived law families over
finite ambients "where the required capabilities exist", in the library (not
test-only loops). Each family below is exercised per-case today by the local
witness checkers in `check.h` (`test_check_*` unit programs); the leftover is a
library exhaustive sweep that enumerates the relevant cases automatically.

- SCENARIO exhaustive iso laws: GIVEN a finite ambient with equality WHEN the
  exhaustive checker runs over every isomorph pair (f, g) present (where
  f o g == 1 and g o f == 1) THEN every iso law case passes and is counted.
- SCENARIO exhaustive split mono/epi: GIVEN a finite ambient with equality WHEN
  the checker runs over every (section, retraction) pair present THEN every
  split law case (retraction o section == 1) passes and is counted.
- SCENARIO exhaustive functor identity: GIVEN a functor F and a finite
  enumeration of its source objects WHEN the checker runs THEN
  F(1_A) == 1_{F(A)} passes for every object A and is counted.
- SCENARIO exhaustive functor composition: GIVEN a functor F and finite
  enumerations of source morphisms WHEN the checker runs THEN
  F(g o f) == F(g) o F(f) passes for every composable (g, f) and is counted.
- SCENARIO exhaustive naturality: GIVEN a natural transformation eta : F => G
  and a finite enumeration of source morphisms WHEN the checker runs THEN
  G(f) o eta_A == eta_B o F(f) passes for every f : A -> B and is counted.
- SCENARIO exhaustive cone/cocone: GIVEN a cone/cocone over a finite shape
  diagram and a finite enumeration of the shape morphisms WHEN the checker runs
  THEN every commutation cell is verified and is counted.
- SCENARIO missing capability refused: GIVEN any exhaustive law-family sweep
  whose required capability (equality or a shape/enumeration) is absent WHEN it
  runs THEN it returns CAT89_NOT_SUPPORTED, never a silent partial pass.

## Where this is recorded today

The per-case witness checkers that these sweeps would drive exist in `check.h`:
`cat89_check_iso`, `cat89_check_split_mono`, `cat89_check_split_epi`,
`cat89_check_functor_identity`, `cat89_check_functor_composition`,
`cat89_check_naturality`, `cat89_check_cone`, `cat89_check_cocone`. The
exhaustive collection of the candidate pairs/functors/nats/cones they operate on
(over the finite ambients) is the outstanding library work.
