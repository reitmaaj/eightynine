/* test_equal_compare.c - byte-exact equality and lexicographic ordering. */

#include "str89_test.h"

static str89_view view_of_bytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "equal: view accepted");
    return v;
}

static void test_equal_basic(void)
{
    static const unsigned char abc[] = "abc";
    static const unsigned char abc2[] = "abc";
    static const unsigned char abd[] = "abd";
    static const unsigned char ab[] = "ab";
    static const unsigned char zbc[] = "zbc";
    static const unsigned char one = 0x00;
    static const unsigned char a_nul_b[] = {'a', 0x00, 'b'};
    static const unsigned char a_nul_c[] = {'a', 0x00, 'c'};
    str89_view empty;
    str89_view zero;
    str89_view a;
    str89_view b;

    empty = view_of_bytes(NULL, 0);
    zero = view_of_bytes(&one, 0);
    str89_test_check(str89_view_equal(empty, empty) != 0, "equal: empty");
    str89_test_check(str89_view_equal(empty, zero) != 0,
                     "equal: empty vs zero-length storage");

    a = view_of_bytes(abc, 3);
    str89_test_check(str89_view_equal(a, a) != 0, "equal: same object");
    b = view_of_bytes(abc2, 3);
    str89_test_check(str89_view_equal(a, b) != 0, "equal: same bytes");
    b = view_of_bytes(abd, 3);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: last byte");
    b = view_of_bytes(ab, 2);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: length");
    b = view_of_bytes(zbc, 3);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: first byte");

    a = view_of_bytes(a_nul_b, 3);
    b = view_of_bytes(a_nul_c, 3);
    str89_test_check(str89_view_equal(a, a) != 0, "equal: embedded NUL self");
    str89_test_check(str89_view_equal(a, b) == 0, "equal: embedded NUL diff");
}

static void test_equal_no_transform(void)
{
    static const unsigned char precomposed[] = {0xC3, 0xA9};      /* U+00E9 */
    static const unsigned char decomposed[] = {0x65, 0xCC, 0x81}; /* e+acute */
    static const unsigned char upper_a[] = "A";
    static const unsigned char lower_a[] = "a";
    static const unsigned char sharp_s[] = {0xC3, 0x9F}; /* U+00DF */
    static const unsigned char ss[] = "ss";
    str89_view a;
    str89_view b;

    a = view_of_bytes(precomposed, 2);
    b = view_of_bytes(decomposed, 3);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: no normalization");
    str89_test_check(str89_view_compare(a, b) != 0,
                     "compare: no canonical equivalence");

    a = view_of_bytes(upper_a, 1);
    b = view_of_bytes(lower_a, 1);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: no case folding");
    str89_test_check(str89_view_compare(a, b) < 0, "compare: A < a");

    a = view_of_bytes(sharp_s, 2);
    b = view_of_bytes(ss, 2);
    str89_test_check(str89_view_equal(a, b) == 0, "equal: sharp s != ss");
}

static void test_compare_basic(void)
{
    static const unsigned char empty[] = "";
    static const unsigned char a[] = "a";
    static const unsigned char aa[] = "aa";
    static const unsigned char b[] = "b";
    static const unsigned char nul[] = {0x00};
    static const unsigned char one[] = {0x01};
    static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
    str89_view vempty;
    str89_view va;
    str89_view vaa;
    str89_view vb;
    str89_view vnul;
    str89_view vone;
    str89_view veuro;

    vempty = view_of_bytes(empty, 0);
    va = view_of_bytes(a, 1);
    vaa = view_of_bytes(aa, 2);
    vb = view_of_bytes(b, 1);
    vnul = view_of_bytes(nul, 1);
    vone = view_of_bytes(one, 1);
    veuro = view_of_bytes(euro, 3);

    str89_test_check(str89_view_compare(vempty, vempty) == 0, "compare: empty");
    str89_test_check(str89_view_compare(vempty, va) < 0, "compare: empty < a");
    str89_test_check(str89_view_compare(va, vaa) < 0, "compare: a < aa");
    str89_test_check(str89_view_compare(va, vb) < 0, "compare: a < b");
    str89_test_check(str89_view_compare(va, vaa) < 0, "compare: prefix");
    str89_test_check(str89_view_compare(vnul, vone) < 0, "compare: 00 < 01");
    str89_test_check(str89_view_compare(va, veuro) < 0,
                     "compare: ASCII < multibyte");
}

static void test_compare_laws(void)
{
    static const unsigned char s0[] = "";
    static const unsigned char s1[] = "a";
    static const unsigned char s2[] = "ab";
    static const unsigned char s3[] = {0x00, 0x41};
    static const unsigned char s4[] = {0xC3, 0xA9};
    str89_view v[5];
    int c;
    int d;
    int e;
    size_t i;
    size_t j;
    size_t k;

    v[0] = view_of_bytes(s0, 0);
    v[1] = view_of_bytes(s1, 1);
    v[2] = view_of_bytes(s2, 2);
    v[3] = view_of_bytes(s3, 2);
    v[4] = view_of_bytes(s4, 2);

    for (i = 0; i < 5; i += 1)
    {
        str89_test_check(str89_view_compare(v[i], v[i]) == 0,
                         "compare: reflexive");
    }
    for (i = 0; i < 5; i += 1)
    {
        for (j = 0; j < 5; j += 1)
        {
            c = str89_view_compare(v[i], v[j]);
            d = str89_view_compare(v[j], v[i]);
            if (c < 0)
            {
                str89_test_check(d > 0, "compare: antisymmetric");
            }
            if (c > 0)
            {
                str89_test_check(d < 0, "compare: antisymmetric");
            }
            if (c == 0)
            {
                str89_test_check(d == 0, "compare: antisymmetric");
            }
        }
    }
    for (i = 0; i < 5; i += 1)
    {
        for (j = 0; j < 5; j += 1)
        {
            for (k = 0; k < 5; k += 1)
            {
                c = str89_view_compare(v[i], v[j]);
                d = str89_view_compare(v[j], v[k]);
                e = str89_view_compare(v[i], v[k]);
                if (c <= 0)
                {
                    if (d <= 0)
                    {
                        str89_test_check(e <= 0, "compare: transitive");
                    }
                }
            }
        }
    }
}

int main(void)
{
    test_equal_basic();
    test_equal_no_transform();
    test_compare_basic();
    test_compare_laws();
    return str89_test_report();
}
