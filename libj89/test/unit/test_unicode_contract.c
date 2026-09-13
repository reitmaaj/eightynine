/* test_unicode_contract.c - libu89/libj89 raw-UTF-8 agreement.
 *
 * For byte sequences that contain no raw ASCII quote, backslash, or control
 * scalar U+0000..U+001F (which JSON owns independently of UTF-8 validity):
 *
 *     u89_utf8_valid(s) == 1  =>  j89_parse("\"" + s + "\"") succeeds
 *     u89_utf8_valid(s) == 0  =>  j89_parse("\"" + s + "\"") fails
 *
 * This encodes the architectural contract that j89 delegates scalar validity
 * to libu89 rather than reimplementing it. */
#include <stdio.h>
#include <string.h>

#include "j89.h"
#include "u89.h"

static int failures;

static void fail(const char *label)
{
    fprintf(stderr, "FAIL: %s\n", label);
    failures = failures + 1;
}

struct seq_case
{
    const unsigned char *s;
    size_t n;
    const char *label;
};

static const unsigned char S_ASCII[] = {'a', 'b', 'c'};
static const unsigned char S_U007F[] = {0x7F};
static const unsigned char S_U0080[] = {0xC2, 0x80};
static const unsigned char S_U07FF[] = {0xDF, 0xBF};
static const unsigned char S_U0800[] = {0xE0, 0xA0, 0x80};
static const unsigned char S_UD7FF[] = {0xED, 0x9F, 0xBF};
static const unsigned char S_UE000[] = {0xEE, 0x80, 0x80};
static const unsigned char S_UFFFF[] = {0xEF, 0xBF, 0xBF};
static const unsigned char S_U10000[] = {0xF0, 0x90, 0x80, 0x80};
static const unsigned char S_U10FFFF[] = {0xF4, 0x8F, 0xBF, 0xBF};
static const unsigned char S_EMOJI[] = {0xF0, 0x9F, 0x98, 0x80};
static const unsigned char S_MIXED[] = {'x',  0xC3, 0xA9, 0xE2, 0x82,
                                        0xAC, 0xF0, 0x9F, 0x98, 0x80};
static const unsigned char S_LONE80[] = {0x80};
static const unsigned char S_LONEBF[] = {0xBF};
static const unsigned char S_OVER2_C0[] = {0xC0, 0xAF};
static const unsigned char S_OVER2_C1[] = {0xC1, 0xBF};
static const unsigned char S_OVER3[] = {0xE0, 0x80, 0x80};
static const unsigned char S_SURR[] = {0xED, 0xA0, 0x80};
static const unsigned char S_OVER4[] = {0xF0, 0x80, 0x80, 0x80};
static const unsigned char S_ABOVE[] = {0xF4, 0x90, 0x80, 0x80};
static const unsigned char S_F5[] = {0xF5, 0x80, 0x80, 0x80};
static const unsigned char S_FE[] = {0xFE};
static const unsigned char S_FF[] = {0xFF};
static const unsigned char S_TRUNC2[] = {0xC2};
static const unsigned char S_TRUNC3[] = {0xE2, 0x82};
static const unsigned char S_TRUNC4[] = {0xF0, 0x9F, 0x8C};
static const unsigned char S_BADCONT2[] = {0xC2, 0x41};
static const unsigned char S_BADCONT3[] = {0xE2, 0x82, 0x41};
static const unsigned char S_BADCONT4[] = {0xF0, 0x9F, 0x8C, 0x41};

static const struct seq_case CASES[] = {
    {S_ASCII, sizeof S_ASCII, "ascii"},
    {S_U007F, sizeof S_U007F, "U+007F"},
    {S_U0080, sizeof S_U0080, "U+0080"},
    {S_U07FF, sizeof S_U07FF, "U+07FF"},
    {S_U0800, sizeof S_U0800, "U+0800"},
    {S_UD7FF, sizeof S_UD7FF, "U+D7FF"},
    {S_UE000, sizeof S_UE000, "U+E000"},
    {S_UFFFF, sizeof S_UFFFF, "U+FFFF"},
    {S_U10000, sizeof S_U10000, "U+10000"},
    {S_U10FFFF, sizeof S_U10FFFF, "U+10FFFF"},
    {S_EMOJI, sizeof S_EMOJI, "U+1F600"},
    {S_MIXED, sizeof S_MIXED, "mixed"},
    {S_LONE80, sizeof S_LONE80, "lone continuation 80"},
    {S_LONEBF, sizeof S_LONEBF, "lone continuation BF"},
    {S_OVER2_C0, sizeof S_OVER2_C0, "C0 overlong"},
    {S_OVER2_C1, sizeof S_OVER2_C1, "C1 overlong"},
    {S_OVER3, sizeof S_OVER3, "E0 overlong"},
    {S_SURR, sizeof S_SURR, "encoded surrogate"},
    {S_OVER4, sizeof S_OVER4, "F0 overlong"},
    {S_ABOVE, sizeof S_ABOVE, "above U+10FFFF"},
    {S_F5, sizeof S_F5, "F5 lead"},
    {S_FE, sizeof S_FE, "FE lead"},
    {S_FF, sizeof S_FF, "FF lead"},
    {S_TRUNC2, sizeof S_TRUNC2, "truncated 2"},
    {S_TRUNC3, sizeof S_TRUNC3, "truncated 3"},
    {S_TRUNC4, sizeof S_TRUNC4, "truncated 4"},
    {S_BADCONT2, sizeof S_BADCONT2, "bad continuation 2"},
    {S_BADCONT3, sizeof S_BADCONT3, "bad continuation 3"},
    {S_BADCONT4, sizeof S_BADCONT4, "bad continuation 4"}};

static void check_agreement(const struct seq_case *tc)
{
    char buf[64];
    j89_arena a;
    j89_len root;
    int valid;
    int parsed;
    size_t i;

    if (tc->n + 2 > sizeof buf)
    {
        fail("case buffer too small");
        return;
    }
    buf[0] = '"';
    for (i = 0; i < tc->n; i = i + 1)
    {
        buf[1 + i] = (char)tc->s[i];
    }
    buf[1 + tc->n] = '"';

    valid = u89_utf8_valid(tc->s, tc->n);
    j89_arena_init(&a);
    root = j89_parse(buf, tc->n + 2, &a);
    parsed = (root != J89_BAD);
    if (valid != parsed)
    {
        fail(tc->label);
        j89_arena_destroy(&a);
        return;
    }
    if (valid)
    {
        const char *v;
        j89_len n;

        v = j89_string_value(&a, root);
        n = j89_string_length(&a, root);
        if (n != tc->n || memcmp(v, tc->s, tc->n) != 0)
        {
            fail(tc->label);
        }
    }
    j89_arena_destroy(&a);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof CASES / sizeof CASES[0]; i = i + 1)
    {
        check_agreement(&CASES[i]);
    }
    if (failures != 0)
    {
        fprintf(stderr, "test_unicode_contract: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_unicode_contract: OK\n");
    return 0;
}
