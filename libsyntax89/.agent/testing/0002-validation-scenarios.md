# 0002 — validation, freeze, and traversal scenarios

SCENARIO freeze a single-node graph
  GIVEN a BUILDING graph with one node set as root
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_OK`
  AND `syntax89_is_frozen` is 1

SCENARIO freeze without a root
  GIVEN a BUILDING graph with nodes and no root
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_EGRAPH`
  AND the graph remains BUILDING and unchanged

SCENARIO freeze an unreachable node
  GIVEN root → A and a disconnected node B
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_EGRAPH`
  AND the graph remains BUILDING

SCENARIO freeze a self-cycle
  GIVEN A → A with A as root
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_ECYCLE`
  AND the graph remains BUILDING

SCENARIO freeze an indirect cycle
  GIVEN A → B → C → A with A as root
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_ECYCLE`

SCENARIO freeze a shared DAG
  GIVEN ADD with LEFT→X and RIGHT→X
  WHEN `syntax89_freeze` is called
  THEN the result is `SYNTAX89_OK`

SCENARIO freeze twice
  GIVEN a FROZEN graph
  WHEN `syntax89_freeze` is called again
  THEN the result is `SYNTAX89_OK` and nothing changes

SCENARIO validation never mutates
  GIVEN any BUILDING or FROZEN graph
  WHEN `syntax89_validate` is called repeatedly
  THEN the observable snapshot is unchanged every time

SCENARIO validate agrees with freeze
  GIVEN any graph
  WHEN `syntax89_validate` succeeds
  THEN `syntax89_freeze` succeeds or returns `SYNTAX89_ENOMEM`
  AND when validation returns `SYNTAX89_ECYCLE` freeze returns `SYNTAX89_ECYCLE`

SCENARIO node-once traversal of a DAG
  GIVEN A with LEFT→B, RIGHT→C, B→D, C→D
  WHEN `syntax89_walk_nodes_pre` is called
  THEN the visited sequence is A B D C
  AND `syntax89_walk_nodes_post` visits D B C A

SCENARIO occurrence traversal of a DAG
  GIVEN A with LEFT→B, RIGHT→C, B→D, C→D
  WHEN `syntax89_walk_edges_post` is called
  THEN D is visited twice, once per incoming edge

SCENARIO callback early stop
  GIVEN a walk whose callback returns `SYNTAX89_END`
  WHEN the walk runs
  THEN the walk returns `SYNTAX89_OK` immediately

SCENARIO callback error propagation
  GIVEN a walk whose callback returns `SYNTAX89_EINVAL`
  WHEN the walk runs
  THEN the walk returns `SYNTAX89_EINVAL`

SCENARIO traversal of a cyclic BUILDING graph
  GIVEN A → B → A
  WHEN any walk starts at A
  THEN the result is `SYNTAX89_ECYCLE`
