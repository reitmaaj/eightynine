/* test_parser_fixture.c - parser-shaped producer versus direct construction.
 *
 * The library never parses; this fixture proves that a parser-like emitter and
 * a direct builder produce the same abstract syntax for `f(1, x + 2)`, with
 * token text kept in an external side table keyed by node id. */

#include "syntax89_fixtures.h"
#include "syntax89_internal.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

enum
{
    TOK_NAME = 1,
    TOK_INT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_PLUS,
    TOK_EOF
};

struct token
{
    int kind;
    const char *text;
    unsigned long begin;
    unsigned long end;
};

struct parser_state
{
    syntax89_graph *g;
    const struct token *toks;
    unsigned long count;
    unsigned long pos;
};

static struct token next_token(const struct parser_state *p)
{
    return p->toks[p->pos];
}

static syntax89_id parse_expr(struct parser_state *p);

static syntax89_id emit_node(struct parser_state *p, syntax89_kind kind,
                             const struct token *tok)
{
    syntax89_id id;
    syntax89_span span;

    span.source = 1;
    span.begin = tok->begin;
    span.end = tok->end;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(p->g, kind, span, &id));
    return id;
}

static syntax89_id parse_atom(struct parser_state *p)
{
    struct token tok;
    syntax89_id id;

    tok = next_token(p);
    if (tok.kind == TOK_INT)
    {
        id = emit_node(p, K_INT, &tok);
        p->pos += 1;
        return id;
    }
    if (tok.kind == TOK_NAME)
    {
        id = emit_node(p, K_NAME, &tok);
        p->pos += 1;
        return id;
    }
    if (tok.kind == TOK_LPAREN)
    {
        p->pos += 1;
        id = parse_expr(p);
        tok = next_token(p);
        T_ASSERT(tok.kind == TOK_RPAREN);
        p->pos += 1;
        return id;
    }
    T_ASSERT(0);
    return SYNTAX89_ID_NONE;
}

static syntax89_id parse_expr(struct parser_state *p)
{
    syntax89_id lhs;
    syntax89_id rhs;
    syntax89_id add;
    struct token tok;

    lhs = parse_atom(p);
    tok = next_token(p);
    if (tok.kind != TOK_PLUS)
    {
        return lhs;
    }
    p->pos += 1;
    rhs = parse_atom(p);
    add = emit_node(p, K_ADD, &tok);
    T_OK(syntax89_add_child(p->g, add, R_LEFT, lhs));
    T_OK(syntax89_add_child(p->g, add, R_RIGHT, rhs));
    return add;
}

static syntax89_id parse_call(struct parser_state *p)
{
    syntax89_id call;
    syntax89_id callee;
    syntax89_id arg;
    struct token tok;

    callee = parse_atom(p);
    tok = next_token(p);
    T_ASSERT(tok.kind == TOK_LPAREN);
    p->pos += 1;
    call = emit_node(p, K_CALL, &tok);
    T_OK(syntax89_add_child(p->g, call, R_CALLEE, callee));
    arg = parse_expr(p);
    T_OK(syntax89_add_child(p->g, call, R_ARG, arg));
    tok = next_token(p);
    while (tok.kind == TOK_COMMA)
    {
        p->pos += 1;
        arg = parse_expr(p);
        T_OK(syntax89_add_child(p->g, call, R_ARG, arg));
        tok = next_token(p);
    }
    T_ASSERT(tok.kind == TOK_RPAREN);
    p->pos += 1;
    T_OK(syntax89_set_root(p->g, call));
    return call;
}

static syntax89_id build_via_parser(syntax89_graph *g)
{
    static const struct token toks[] = {
        {TOK_NAME, "f", 0, 1}, {TOK_LPAREN, "(", 1, 2},
        {TOK_INT, "1", 2, 3},  {TOK_COMMA, ",", 3, 4},
        {TOK_NAME, "x", 5, 6}, {TOK_PLUS, "+", 7, 8},
        {TOK_INT, "2", 9, 10}, {TOK_RPAREN, ")", 10, 11},
        {TOK_EOF, "", 11, 11},
    };
    struct parser_state p;

    T_OK(syntax89_init(g, NULL));
    p.g = g;
    p.toks = toks;
    p.count = 9;
    p.pos = 0;
    return parse_call(&p);
}

static syntax89_id build_direct(syntax89_graph *g)
{
    syntax89_id call;
    syntax89_id fn;
    syntax89_id one;
    syntax89_id add;
    syntax89_id x;
    syntax89_id two;

    T_OK(syntax89_init(g, NULL));
    call = syntax89_fixture_node(g, K_CALL, 1, 2);
    fn = syntax89_fixture_node(g, K_NAME, 0, 1);
    one = syntax89_fixture_node(g, K_INT, 2, 3);
    add = syntax89_fixture_node(g, K_ADD, 7, 8);
    x = syntax89_fixture_node(g, K_NAME, 5, 6);
    two = syntax89_fixture_node(g, K_INT, 9, 10);
    syntax89_fixture_edge(g, call, R_CALLEE, fn);
    syntax89_fixture_edge(g, call, R_ARG, one);
    syntax89_fixture_edge(g, call, R_ARG, add);
    syntax89_fixture_edge(g, add, R_LEFT, x);
    syntax89_fixture_edge(g, add, R_RIGHT, two);
    T_OK(syntax89_set_root(g, call));
    return call;
}

/* Compare two subtree shapes by kind, span, and ordered roles, independent of
 * node ids (the two producers allocate nodes in different orders). */
static void compare_subtree(const syntax89_graph *a, syntax89_id ai,
                            const syntax89_graph *b, syntax89_id bi)
{
    syntax89_node_info ia;
    syntax89_node_info ib;
    syntax89_role ra;
    syntax89_role rb;
    syntax89_id ca;
    syntax89_id cb;
    unsigned long i;

    T_OK(syntax89_node(a, ai, &ia));
    T_OK(syntax89_node(b, bi, &ib));
    T_EQ_UL(ia.kind, ib.kind);
    T_EQ_UL(ia.span.begin, ib.span.begin);
    T_EQ_UL(ia.span.end, ib.span.end);
    T_EQ_UL(syntax89_child_count(a, ai), syntax89_child_count(b, bi));
    for (i = 0; i < syntax89_child_count(a, ai); ++i)
    {
        T_OK(syntax89_child_at(a, ai, i, &ra, &ca));
        T_OK(syntax89_child_at(b, bi, i, &rb, &cb));
        T_EQ_UL(ra, rb);
        compare_subtree(a, ca, b, cb);
    }
}

static void test_parser_and_direct_agree(void)
{
    syntax89_graph gp;
    syntax89_graph gd;
    syntax89_id call;
    syntax89_role role;
    syntax89_id child;

    call = build_via_parser(&gp);
    build_direct(&gd);
    T_EQ_UL(syntax89_node_count(&gp), 6);
    T_EQ_UL(syntax89_edge_count(&gp), 5);
    T_EQ_UL(syntax89_node_count(&gd), 6);
    T_EQ_UL(syntax89_edge_count(&gd), 5);
    compare_subtree(&gp, call, &gd, syntax89_root(&gd));
    T_EQ_UL(syntax89_child_count(&gp, call), 3);
    T_OK(syntax89_child_at(&gp, call, 0, &role, &child));
    T_EQ_UL(role, R_CALLEE);
    T_EQ_UL(child, 1);
    T_OK(syntax89_child_at(&gp, call, 1, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, 3);
    T_OK(syntax89_child_at(&gp, call, 2, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, 6);
    T_EQ_UL(syntax89_child_count_role(&gp, call, R_ARG), 2);
    T_OK(syntax89_freeze(&gp));
    T_OK(syntax89_freeze(&gd));
    syntax89_destroy(&gp);
    syntax89_destroy(&gd);
}

static void test_external_text_side_table(void)
{
    syntax89_graph g;
    const char *text[7];

    build_via_parser(&g);
    text[1] = "f";
    text[3] = "1";
    text[4] = "x";
    text[5] = "2";
    T_ASSERT(text[1][0] == 'f');
    T_ASSERT(text[3][0] == '1');
    T_ASSERT(text[4][0] == 'x');
    T_ASSERT(text[5][0] == '2');
    T_EQ_UL(syntax89__node_at(&g, 1)->kind, K_NAME);
    T_EQ_UL(syntax89__node_at(&g, 3)->kind, K_INT);
    T_EQ_UL(syntax89__node_at(&g, 4)->kind, K_NAME);
    T_EQ_UL(syntax89__node_at(&g, 5)->kind, K_INT);
    syntax89_destroy(&g);
}

int main(void)
{
    test_parser_and_direct_agree();
    test_external_text_side_table();
    return syntax89_test_report("test_parser_fixture");
}
