/* test_find.c - byte substring search. */

#include "str89_test.h"

static str89_view view_of_bytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "find: view accepted");
    return v;
}

static void test_find_positions(void)
{
    static const unsigned char hay[] = "abcabc";
    static const unsigned char needle[] = "abc";
    static const unsigned char absent[] = "xyz";
    static const unsigned char c[] = "c";
    str89_view h;
    str89_view n;
    size_t at;
    int r;

    h = view_of_bytes(hay, 6);
    n = view_of_bytes(needle, 3);

    r = str89_view_find(h, n, 0, &at);
    str89_test_check_status(r, STR89_OK, "find: present");
    str89_test_check(at == 0, "find: at start");
    r = str89_view_find(h, n, 1, &at);
    str89_test_check_status(r, STR89_OK, "find: from 1");
    str89_test_check(at == 3, "find: repeated at 3");
    r = str89_view_find(h, n, 4, &at);
    str89_test_check_status(r, STR89_OK, "find: from 4");
    str89_test_check(at == STR89_NPOS, "find: absent after 4");
    r = str89_view_find(h, view_of_bytes(absent, 3), 0, &at);
    str89_test_check_status(r, STR89_OK, "find: absent");
    str89_test_check(at == STR89_NPOS, "find: absent NPOS");
    r = str89_view_find(h, view_of_bytes(c, 1), 0, &at);
    str89_test_check_status(r, STR89_OK, "find: single byte");
    str89_test_check(at == 2, "find: single byte at 2");
    r = str89_view_find(h, view_of_bytes(c, 1), 3, &at);
    str89_test_check_status(r, STR89_OK, "find: from match");
    str89_test_check(at == 5, "find: next match");
}

static void test_find_empty(void)
{
    static const unsigned char hay[] = "abc";
    str89_view h;
    str89_view empty;
    size_t at;
    int r;

    h = view_of_bytes(hay, 3);
    empty = view_of_bytes(NULL, 0);

    r = str89_view_find(h, empty, 0, &at);
    str89_test_check_status(r, STR89_OK, "find: empty needle");
    str89_test_check(at == 0, "find: empty at 0");
    r = str89_view_find(h, empty, 3, &at);
    str89_test_check_status(r, STR89_OK, "find: empty at end");
    str89_test_check(at == 3, "find: empty offset");
    r = str89_view_find(view_of_bytes(NULL, 0), empty, 0, &at);
    str89_test_check_status(r, STR89_OK, "find: empty in empty");
    str89_test_check(at == 0, "find: empty in empty at 0");
}

static void test_find_multibyte(void)
{
    static const unsigned char hay[] = {0x41, 0xE2, 0x82, 0xAC,
                                        0x42, 0xC2, 0xA2};
    static const unsigned char euro[] = {0xE2, 0x82, 0xAC};
    static const unsigned char cent[] = {0xC2, 0xA2};
    str89_view h;
    size_t at;
    int b;
    int r;

    h = view_of_bytes(hay, 7);
    r = str89_view_find(h, view_of_bytes(euro, 3), 0, &at);
    str89_test_check_status(r, STR89_OK, "find: multibyte needle");
    str89_test_check(at == 1, "find: multibyte at 1");
    b = str89_view_is_boundary(h, at);
    str89_test_check(b == 1, "find: match is a scalar boundary");

    r = str89_view_find(h, view_of_bytes(cent, 2), 0, &at);
    str89_test_check_status(r, STR89_OK, "find: cent needle");
    str89_test_check(at == 5, "find: cent at 5");
    b = str89_view_is_boundary(h, at);
    str89_test_check(b == 1, "find: cent match is a boundary");

    /* A valid needle never begins with a continuation byte, so no match can
     * begin inside the euro scalar: from inside it the search is rejected. */
    at = 99;
    r = str89_view_find(h, view_of_bytes(cent, 2), 2, &at);
    str89_test_check_status(r, STR89_EBOUND, "find: from inside scalar");
    str89_test_check(at == 99, "find: inside scalar leaves offset");
}

static void test_find_embedded_nul(void)
{
    static const unsigned char hay[] = {'a', 0x00, 'b', 'c'};
    static const unsigned char needle[] = {0x00, 'b'};
    str89_view h;
    size_t at;
    int r;

    h = view_of_bytes(hay, 4);
    r = str89_view_find(h, view_of_bytes(needle, 2), 0, &at);
    str89_test_check_status(r, STR89_OK, "find: NUL needle");
    str89_test_check(at == 1, "find: NUL at 1");
}

static void test_find_bad_from(void)
{
    static const unsigned char hay[] = {0x41, 0xE2, 0x82, 0xAC};
    str89_view h;
    size_t at;
    int r;

    h = view_of_bytes(hay, 4);
    at = 99;
    r = str89_view_find(h, view_of_bytes(NULL, 0), 5, &at);
    str89_test_check_status(r, STR89_ERANGE, "find: from past end");
    str89_test_check(at == 99, "find: bad from leaves offset");
    r = str89_view_find(h, view_of_bytes(NULL, 0), 2, &at);
    str89_test_check_status(r, STR89_EBOUND, "find: from inside scalar");
    str89_test_check(at == 99, "find: EBOUND leaves offset");
}

int main(void)
{
    test_find_positions();
    test_find_empty();
    test_find_multibyte();
    test_find_embedded_nul();
    test_find_bad_from();
    return str89_test_report();
}
