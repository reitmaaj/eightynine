/* test_boundary.c - scalar boundary detection. */

#include "str89_test.h"

/* bytes: 41 E2 82 AC F0 90 8D 88 00
 * scalars: A, U+20AC, U+10348, U+0000
 * boundaries: 0, 1, 4, 8, 9 */
static const unsigned char mixed[] = {0x41, 0xE2, 0x82, 0xAC, 0xF0,
                                      0x90, 0x8D, 0x88, 0x00};

static void test_boundary_table(void)
{
    static const int want[] = {1, 1, 0, 0, 1, 0, 0, 0, 1, 1};
    str89_view v;
    int r;
    size_t i;
    int b;

    r = str89_view_init(&v, mixed, sizeof(mixed));
    str89_test_check_status(r, STR89_OK, "boundary: view accepted");
    for (i = 0; i <= sizeof(mixed); i += 1)
    {
        b = str89_view_is_boundary(v, i);
        str89_test_check(b == want[i], "boundary: table entry");
    }
}

static void test_boundary_out_of_range(void)
{
    str89_view v;
    int r;
    int b;

    r = str89_view_init(&v, mixed, sizeof(mixed));
    str89_test_check_status(r, STR89_OK, "boundary: view accepted");
    b = str89_view_is_boundary(v, sizeof(mixed) + 1);
    str89_test_check(b == 0, "boundary: len+1 is not a boundary");
    b = str89_view_is_boundary(v, STR89_NPOS);
    str89_test_check(b == 0, "boundary: SIZE_MAX is not a boundary");
}

static void test_boundary_empty(void)
{
    str89_view v;
    int r;
    int b;

    r = str89_view_init(&v, NULL, 0);
    str89_test_check_status(r, STR89_OK, "boundary: empty view");
    b = str89_view_is_boundary(v, 0);
    str89_test_check(b == 1, "boundary: empty offset 0");
    b = str89_view_is_boundary(v, 1);
    str89_test_check(b == 0, "boundary: empty offset 1");
}

static void test_boundary_all_ascii(void)
{
    str89_view v;
    int r;
    size_t i;
    int b;

    r = str89_view_init(&v, (const unsigned char *)"abc", 3);
    str89_test_check_status(r, STR89_OK, "boundary: ascii view");
    for (i = 0; i <= 3; i += 1)
    {
        b = str89_view_is_boundary(v, i);
        str89_test_check(b == 1, "boundary: ascii offset");
    }
}

int main(void)
{
    test_boundary_table();
    test_boundary_out_of_range();
    test_boundary_empty();
    test_boundary_all_ascii();
    return str89_test_report();
}
