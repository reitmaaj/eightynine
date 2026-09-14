# 0001 — construction and lifecycle scenarios

SCENARIO initialize an empty graph
  GIVEN a zeroed `syntax89_graph`
  WHEN `syntax89_init` is called with a NULL allocator
  THEN the result is `SYNTAX89_OK`
  AND no allocation occurs
  AND the graph is BUILDING with zero nodes, zero edges, and no root

SCENARIO initialize a NULL graph
  GIVEN `g == NULL`
  WHEN `syntax89_init` is called
  THEN the result is `SYNTAX89_EINVAL`

SCENARIO destroy a zeroed graph
  GIVEN a zeroed `syntax89_graph` never initialized
  WHEN `syntax89_destroy` is called
  THEN it is a no-op
  AND a second destroy is also a no-op

SCENARIO destroy a populated graph
  GIVEN a BUILDING or FROZEN graph with nodes and edges
  WHEN `syntax89_destroy` is called
  THEN every allocation is returned through the allocator
  AND the graph is zeroed

SCENARIO add the first node
  GIVEN an empty BUILDING graph
  WHEN `syntax89_add_node` is called with kind 1 and span [0, 0)
  THEN the result is `SYNTAX89_OK`
  AND `*out` is a nonzero id
  AND `syntax89_node_count` is 1
  AND `syntax89_node` reproduces the kind and span exactly

SCENARIO node ids are stable and unique
  GIVEN 100 sequential `syntax89_add_node` calls
  WHEN the ids are collected
  THEN every id is nonzero, distinct, and unchanged by later nodes, edges,
       root changes, and freeze

SCENARIO malformed span rejected
  GIVEN a span with `begin > end`
  WHEN `syntax89_add_node` is called
  THEN the result is `SYNTAX89_EINVAL`
  AND the graph is unchanged

SCENARIO add_node after freeze
  GIVEN a FROZEN graph
  WHEN `syntax89_add_node` is called
  THEN the result is `SYNTAX89_ESTATE`
  AND the graph is unchanged

SCENARIO add_child after freeze
  GIVEN a FROZEN graph
  WHEN `syntax89_add_child` is called
  THEN the result is `SYNTAX89_ESTATE`
  AND the graph is unchanged

SCENARIO set_root after freeze
  GIVEN a FROZEN graph
  WHEN `syntax89_set_root` is called
  THEN the result is `SYNTAX89_ESTATE`
  AND the graph is unchanged

SCENARIO add_child with an unknown endpoint
  GIVEN a BUILDING graph
  WHEN `syntax89_add_child` names an unknown parent or child
  THEN the result is `SYNTAX89_ENODE`
  AND the graph is unchanged

SCENARIO repeated role preserves insertion order
  GIVEN a parent with edges ARG→A, ARG→B, CALLEE→F, ARG→C
  WHEN children are queried by index and by role
  THEN absolute order is A, B, F, C
  AND role order for ARG is A, B, C

SCENARIO shared child
  GIVEN ADD with LEFT→X and RIGHT→X
  WHEN the graph is queried
  THEN both edges name the same id
  AND node_count counts X once and edge_count counts two edges

SCENARIO set root to NONE
  GIVEN a BUILDING graph with a root
  WHEN `syntax89_set_root(g, SYNTAX89_ID_NONE)` is called
  THEN the result is `SYNTAX89_OK`
  AND `syntax89_root` is `SYNTAX89_ID_NONE`
