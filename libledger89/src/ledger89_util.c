/* ledger89_util.c - pure path, name, and width helpers. */

#include <limits.h>
#include <string.h>

#include "ledger89_internal.h"

size_t led89_chunk_size(size_t cap, led89_u64 left)
{
    if ((led89_u64)cap > left)
    {
        return (size_t)left;
    }
    return cap;
}

int led89_path_join(char *out, size_t cap, const char *dir, const char *name)
{
    size_t d;
    size_t n;

    d = strlen(dir);
    n = strlen(name);
    if (d + n + 2u > cap)
    {
        return LEDGER89_ERR_RANGE;
    }
    memcpy(out, dir, d);
    out[d] = '/';
    memcpy(out + d + 1u, name, n + 1u);
    return LEDGER89_OK;
}

static led89_u64 led89_emit_digit(char *out, int i, led89_u64 value)
{
    out[i] = (char)('0' + (int)(value % 10u));
    return value / 10u;
}

void led89_name_format(char *out, led89_u64 index)
{
    int i;

    for (i = 19; i >= 0; --i)
    {
        index = led89_emit_digit(out, i, index);
    }
    out[20] = '.';
    out[21] = 's';
    out[22] = 'e';
    out[23] = 'g';
    out[24] = '\0';
}

static int led89_parse_digit(char c, led89_u64 *value)
{
    led89_u64 digit;
    led89_u64 max;

    if (c < '0')
    {
        return 0;
    }
    if (c > '9')
    {
        return 0;
    }
    digit = (led89_u64)(c - '0');
    max = ~(led89_u64)0;
    if (*value > (max - digit) / 10u)
    {
        return 0;
    }
    *value = (*value * 10u) + digit;
    return 1;
}

int led89_name_parse(const char *name, led89_u64 *index)
{
    led89_u64 v;
    int i;
    int rc;

    if (strlen(name) != 24u)
    {
        return 0;
    }
    v = 0u;
    for (i = 0; i < 20; ++i)
    {
        rc = led89_parse_digit(name[i], &v);
        if (rc == 0)
        {
            return 0;
        }
    }
    if (name[20] != '.')
    {
        return 0;
    }
    if (name[21] != 's')
    {
        return 0;
    }
    if (name[22] != 'e')
    {
        return 0;
    }
    if (name[23] != 'g')
    {
        return 0;
    }
    *index = v;
    return 1;
}

int led89_u64_to_index(led89_u64 v, ledger89_index *out)
{
    if (v > (led89_u64)ULONG_MAX)
    {
        return LEDGER89_ERR_RANGE;
    }
    *out = (ledger89_index)v;
    return LEDGER89_OK;
}

int led89_u64_to_size(led89_u64 v, size_t *out)
{
    if (v > (led89_u64)(size_t)-1)
    {
        return LEDGER89_ERR_RANGE;
    }
    *out = (size_t)v;
    return LEDGER89_OK;
}

int led89_size_to_u64(size_t v, led89_u64 *out)
{
    *out = (led89_u64)v;
    return LEDGER89_OK;
}

int led89_u64_to_u32(led89_u64 v, led89_u32 *out)
{
    if (v > (led89_u64)0xFFFFFFFFu)
    {
        return LEDGER89_ERR_RANGE;
    }
    *out = (led89_u32)v;
    return LEDGER89_OK;
}
