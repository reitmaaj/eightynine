/* test_id.c - unit tests for jrpc89_id_equal. */
#include <stdio.h>

#include <jrpc89.h>

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void expect_equal(const char *label, const jrpc89_id *x,
                         const jrpc89_id *y)
{
    if (!jrpc89_id_equal(x, y))
    {
        fail(label);
    }
}

static void expect_unequal(const char *label, const jrpc89_id *x,
                           const jrpc89_id *y)
{
    if (jrpc89_id_equal(x, y))
    {
        fail(label);
    }
}

static void test_ints(void)
{
    jrpc89_id a;
    jrpc89_id b;
    jrpc89_id c;
    a.kind = JRPC89_ID_INT;
    a.num = 7;
    b.kind = JRPC89_ID_INT;
    b.num = 7;
    c.kind = JRPC89_ID_INT;
    c.num = 8;
    expect_equal("equal ints", &a, &b);
    expect_unequal("different ints", &a, &c);
}

static void test_strings(void)
{
    jrpc89_id a;
    jrpc89_id b;
    jrpc89_id c;
    jrpc89_id d;
    static const char x[] = {'a', '\0', 'b'};
    static const char y[] = {'a', '\0', 'c'};
    a.kind = JRPC89_ID_STRING;
    a.str = "abc";
    a.len = 3;
    b.kind = JRPC89_ID_STRING;
    b.str = "abc";
    b.len = 3;
    c.kind = JRPC89_ID_STRING;
    c.str = "abd";
    c.len = 3;
    d.kind = JRPC89_ID_STRING;
    d.str = "ab";
    d.len = 2;
    expect_equal("equal strings", &a, &b);
    expect_unequal("different strings", &a, &c);
    expect_unequal("different lengths", &a, &d);
    a.str = x;
    a.len = 3;
    b.str = x;
    b.len = 3;
    c.str = y;
    c.len = 3;
    expect_equal("equal embedded nul strings", &a, &b);
    expect_unequal("different embedded nul strings", &a, &c);
}

static void test_kinds(void)
{
    jrpc89_id i;
    jrpc89_id s;
    jrpc89_id n;
    jrpc89_id none;
    jrpc89_id none2;
    i.kind = JRPC89_ID_INT;
    i.num = 1;
    s.kind = JRPC89_ID_STRING;
    s.str = "1";
    s.len = 1;
    n.kind = JRPC89_ID_NULL;
    none.kind = JRPC89_ID_NONE;
    none2.kind = JRPC89_ID_NONE;
    expect_unequal("int vs string", &i, &s);
    expect_unequal("int vs null", &i, &n);
    expect_equal("null vs null", &n, &n);
    expect_equal("none vs none", &none, &none2);
    expect_unequal("none vs null", &none, &n);
}

static void test_nulls(void)
{
    jrpc89_id a;
    a.kind = JRPC89_ID_INT;
    a.num = 1;
    expect_unequal("NULL left", (const jrpc89_id *)0, &a);
    expect_unequal("NULL right", &a, (const jrpc89_id *)0);
    expect_unequal("both NULL", (const jrpc89_id *)0, (const jrpc89_id *)0);
}

int main(void)
{
    test_ints();
    test_strings();
    test_kinds();
    test_nulls();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_id: ok\n");
    return 0;
}
