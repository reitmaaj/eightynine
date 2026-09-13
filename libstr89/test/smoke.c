/* smoke.c - one end-to-end path: validate, copy, build, take, compare, free. */

#include <string.h>

#include "str89_test.h"

int main(void)
{
    static const unsigned char text[] = "h\xC3\xA9llo"; /* h, U+00E9, llo */
    str89_view v;
    str89 s;
    str89 taken;
    str89_buf b;
    int r;

    str89_init(&s);
    str89_init(&taken);
    str89_buf_init(&b);

    r = str89_view_init(&v, text, sizeof(text) - 1);
    str89_test_check(r == STR89_OK, "smoke: view init accepts valid UTF-8");
    str89_test_valid_view(v, "smoke: view is valid");

    r = str89_from_view(&s, NULL, v);
    str89_test_check(r == STR89_OK, "smoke: from_view copies");
    str89_test_valid_str(&s, "smoke: string is valid");
    str89_test_check(str89_view_equal(str89_view_of(&s), v) != 0,
                     "smoke: string equals source view");

    r = str89_buf_append(&b, NULL, v);
    str89_test_check(r == STR89_OK, "smoke: append");
    str89_test_valid_buf(&b, "smoke: buffer is valid");

    r = str89_take(&taken, &b);
    str89_test_check(r == STR89_OK, "smoke: take");
    str89_test_check(str89_view_equal(str89_view_of(&taken), v) != 0,
                     "smoke: taken equals source");
    str89_test_check(b.data == NULL, "smoke: take resets the buffer");

    str89_free(&s, NULL);
    str89_free(&taken, NULL);
    str89_buf_free(&b, NULL);

    return str89_test_report();
}
