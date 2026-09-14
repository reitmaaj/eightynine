/* test_path_tables.c - table-driven white-box coverage for the pure
 * path/name helpers, hex digit helpers, and create-name classification. */

#include <string.h>

#include "test.h"

#include "ledger89_internal.h"

#include "ledger89_file.c"
#include "ledger89_recover.c"
#include "ledger89_util.c"

struct join_case
{
    const char *dir;
    const char *name;
    size_t need;
    const char *out;
};

static const struct join_case join_cases[] = {
    {"a", "b", 4u, "a/b"},
    {"", "x", 3u, "/x"},
    {"a", "", 3u, "a/"},
    {"", "", 2u, "/"},
    {"dir", "part.0000000000000001", 26u, "dir/part.0000000000000001"},
    {"abcdefghijklmnopqrstuvwxyz", "n", 29u, "abcdefghijklmnopqrstuvwxyz/n"}};

struct name_case
{
    led89_u64 id;
    const char *part;
    const char *manifest;
};

static const struct name_case name_cases[] = {
    {(led89_u64)0, "part.0000000000000000", "MANIFEST.0000000000000000"},
    {(led89_u64)1, "part.0000000000000001", "MANIFEST.0000000000000001"},
    {(led89_u64)0xFFFFFFFFu, "part.00000000ffffffff",
     "MANIFEST.00000000ffffffff"},
    {((led89_u64)1 << 32), "part.0000000100000000",
     "MANIFEST.0000000100000000"},
    {~((led89_u64)0), "part.ffffffffffffffff", "MANIFEST.ffffffffffffffff"}};

struct parse_case
{
    const char *name;
    int ok;
    led89_u64 value;
};

static const struct parse_case part_parse_cases[] = {
    {"part.0000000000000000", 1, (led89_u64)0},
    {"part.0000000000000001", 1, (led89_u64)1},
    {"part.00000000ffffffff", 1, (led89_u64)0xFFFFFFFFu},
    {"part.0000000100000000", 1, ((led89_u64)1 << 32)},
    {"part.ffffffffffffffff", 1, ~((led89_u64)0)},
    {"part.123456789abcdef0", 1,
     ((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u},
    {"part.000000000000000g", 0, (led89_u64)0},
    {"part.000000000000000G", 0, (led89_u64)0},
    {"part.000000000000000", 0, (led89_u64)0},
    {"part.00000000000000000", 0, (led89_u64)0},
    {"PART.0000000000000001", 0, (led89_u64)0},
    {"MANIFEST.0000000000000001", 0, (led89_u64)0},
    {"part0000000000000001", 0, (led89_u64)0},
    {"part.0000000000000001x", 0, (led89_u64)0},
    {"part.0000000000000001 ", 0, (led89_u64)0},
    {"", 0, (led89_u64)0},
    {"x", 0, (led89_u64)0}};

static const struct parse_case manifest_parse_cases[] = {
    {"MANIFEST.0000000000000000", 1, (led89_u64)0},
    {"MANIFEST.0000000000000001", 1, (led89_u64)1},
    {"MANIFEST.ffffffffffffffff", 1, ~((led89_u64)0)},
    {"MANIFEST.123456789abcdef0", 1,
     ((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u},
    {"MANIFEST.000000000000000g", 0, (led89_u64)0},
    {"MANIFEST.000000000000000", 0, (led89_u64)0},
    {"MANIFEST.00000000000000000", 0, (led89_u64)0},
    {"manifest.0000000000000001", 0, (led89_u64)0},
    {"MANIFEST0000000000000001", 0, (led89_u64)0},
    {"part.0000000000000001", 0, (led89_u64)0},
    {"MANIFEST.", 0, (led89_u64)0},
    {"", 0, (led89_u64)0}};

struct hex_value_case
{
    char c;
    int expected;
};

static const struct hex_value_case hex_value_cases[] = {
    {'0', 0},  {'1', 1},  {'5', 5},  {'9', 9},  {'a', 10}, {'b', 11}, {'c', 12},
    {'d', 13}, {'e', 14}, {'f', 15}, {'A', -1}, {'F', -1}, {'G', -1}, {'g', -1},
    {'/', -1}, {':', -1}, {'@', -1}, {'`', -1}, {'\0', -1}};

struct hex_digit_case
{
    led89_u64 v;
    unsigned int i;
    char expected;
};

static const struct hex_digit_case hex_digit_cases[] = {
    {(led89_u64)0, 0u, '0'},
    {(led89_u64)0, 15u, '0'},
    {(led89_u64)0xF, 0u, '0'},
    {(led89_u64)0xF, 15u, 'f'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 0u, '1'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 1u, '2'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 2u, '3'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 3u, '4'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 4u, '5'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 5u, '6'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 6u, '7'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 7u, '8'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 8u, '9'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 9u, 'a'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 10u, 'b'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 11u, 'c'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 12u, 'd'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 13u, 'e'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 14u, 'f'},
    {((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u, 15u, '0'},
    {~((led89_u64)0), 0u, 'f'},
    {~((led89_u64)0), 15u, 'f'}};

struct hex_accum_case
{
    led89_u64 value;
    led89_u64 digit;
    led89_u64 expected;
};

static const struct hex_accum_case hex_accum_cases[] = {
    {(led89_u64)0, (led89_u64)0, (led89_u64)0},
    {(led89_u64)0, (led89_u64)1, (led89_u64)1},
    {(led89_u64)1, (led89_u64)2, (led89_u64)0x12},
    {(led89_u64)0xF, (led89_u64)0xF, (led89_u64)0xFF},
    {(led89_u64)0x123, (led89_u64)4, (led89_u64)0x1234},
    {(led89_u64)0x0FFFFFFFFFFFFFFFu, (led89_u64)0,
     (led89_u64)0xFFFFFFFFFFFFFFF0u},
    {~((led89_u64)0), (led89_u64)0xF, ~((led89_u64)0)}};

struct hex16_case
{
    const char *text;
    int ok;
    led89_u64 value;
};

static const struct hex16_case hex16_cases[] = {
    {"0000000000000000", 1, (led89_u64)0},
    {"0000000000000001", 1, (led89_u64)1},
    {"00000000ffffffff", 1, (led89_u64)0xFFFFFFFFu},
    {"0000000100000000", 1, ((led89_u64)1 << 32)},
    {"ffffffffffffffff", 1, ~((led89_u64)0)},
    {"123456789abcdef0", 1,
     ((led89_u64)0x12345678u << 32) | (led89_u64)0x9ABCDEF0u},
    {"000000000000000g", 0, (led89_u64)0},
    {"000000000000000G", 0, (led89_u64)0},
    {"00000000000000 0", 0, (led89_u64)0},
    {"ABCDEFABCDEFABCD", 0, (led89_u64)0}};

struct dot_case
{
    const char *name;
    int expected;
};

static const struct dot_case dot_cases[] = {{".", 1}, {"..", 1}, {"...", 0},
                                            {"", 0},  {".x", 0}, {"x.", 0},
                                            {"x", 0}, {"..x", 0}};

struct kind_case
{
    const char *name;
    int expected;
};

static const struct kind_case kind_cases[] = {{"lock", 0},
                                              {"CURRENT.tmp", 1},
                                              {"part.0000000000000000", 1},
                                              {"part.ffffffffffffffff", 1},
                                              {"MANIFEST.0000000000000001", 1},
                                              {"MANIFEST.ffffffffffffffff", 1},
                                              {"CURRENT", 2},
                                              {"lock.tmp", 2},
                                              {"part.000000000000000g", 2},
                                              {"MANIFEST.000000000000000", 2},
                                              {"part.", 2},
                                              {"CURRENT.tmp.bak", 2},
                                              {"foo", 2},
                                              {"", 2}};

static void test_path_join(void)
{
    size_t i;

    for (i = 0u; i < sizeof join_cases / sizeof join_cases[0]; ++i)
    {
        const struct join_case *c;
        char buf[64];
        size_t cap;

        c = &join_cases[i];
        for (cap = 0u; cap <= c->need + 1u; ++cap)
        {
            int rc;

            memset(buf, '#', sizeof buf);
            rc = led89_path_join(buf, cap, c->dir, c->name);
            if (cap < c->need)
            {
                CHECK_EQ(rc, LEDGER89_ERANGE);
                CHECK_EQ(buf[0], (char)'#');
            }
            else
            {
                CHECK_EQ(rc, LEDGER89_OK);
                CHECK(strcmp(buf, c->out) == 0);
                CHECK_EQ(strlen(buf) + 1u, c->need);
            }
        }
    }
}

static void test_names(void)
{
    size_t i;

    for (i = 0u; i < sizeof name_cases / sizeof name_cases[0]; ++i)
    {
        char buf[LED89_NAME_MAX];

        led89_part_name(buf, name_cases[i].id);
        CHECK(strcmp(buf, name_cases[i].part) == 0);
        led89_manifest_name(buf, name_cases[i].id);
        CHECK(strcmp(buf, name_cases[i].manifest) == 0);
    }
}

static void test_parse(void)
{
    size_t i;

    for (i = 0u; i < sizeof part_parse_cases / sizeof part_parse_cases[0]; ++i)
    {
        led89_u64 out;
        int rc;

        out = ~((led89_u64)0);
        rc = led89_part_name_parse(part_parse_cases[i].name, &out);
        CHECK_EQ(rc, part_parse_cases[i].ok);
        if (rc != 0)
        {
            CHECK(out == part_parse_cases[i].value);
        }
        else
        {
            CHECK(out == ~((led89_u64)0));
        }
    }
    for (i = 0u;
         i < sizeof manifest_parse_cases / sizeof manifest_parse_cases[0]; ++i)
    {
        led89_u64 out;
        int rc;

        out = ~((led89_u64)0);
        rc = led89_manifest_name_parse(manifest_parse_cases[i].name, &out);
        CHECK_EQ(rc, manifest_parse_cases[i].ok);
        if (rc != 0)
        {
            CHECK(out == manifest_parse_cases[i].value);
        }
        else
        {
            CHECK(out == ~((led89_u64)0));
        }
    }
}

static void test_hex_helpers(void)
{
    size_t i;

    for (i = 0u; i < sizeof hex_value_cases / sizeof hex_value_cases[0]; ++i)
    {
        CHECK_EQ(led89_hex_value(hex_value_cases[i].c),
                 hex_value_cases[i].expected);
    }
    for (i = 0u; i < sizeof hex_digit_cases / sizeof hex_digit_cases[0]; ++i)
    {
        CHECK_EQ(led89_hex_digit(hex_digit_cases[i].v, hex_digit_cases[i].i),
                 hex_digit_cases[i].expected);
    }
    for (i = 0u; i < sizeof hex_accum_cases / sizeof hex_accum_cases[0]; ++i)
    {
        CHECK(led89_hex_accum(hex_accum_cases[i].value,
                              hex_accum_cases[i].digit) ==
              hex_accum_cases[i].expected);
    }
    for (i = 0u; i < sizeof hex16_cases / sizeof hex16_cases[0]; ++i)
    {
        led89_u64 out;
        int rc;

        out = ~((led89_u64)0);
        rc = led89_hex16_parse(hex16_cases[i].text, &out);
        CHECK_EQ(rc, hex16_cases[i].ok);
        if (rc != 0)
        {
            CHECK(out == hex16_cases[i].value);
        }
        else
        {
            CHECK(out == ~((led89_u64)0));
        }
    }
}

static void test_dot_name(void)
{
    size_t i;

    for (i = 0u; i < sizeof dot_cases / sizeof dot_cases[0]; ++i)
    {
        CHECK_EQ(led89_dot_name(dot_cases[i].name), dot_cases[i].expected);
    }
}

static void test_create_name_kind(void)
{
    size_t i;

    for (i = 0u; i < sizeof kind_cases / sizeof kind_cases[0]; ++i)
    {
        CHECK_EQ(led89_create_name_kind(kind_cases[i].name),
                 kind_cases[i].expected);
    }
}

int main(void)
{
    test_path_join();
    test_names();
    test_parse();
    test_hex_helpers();
    test_dot_name();
    test_create_name_kind();
    TEST_END;
}
