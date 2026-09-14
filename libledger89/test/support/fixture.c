/* fixture.c - black-box fixture for API tests. */

#include <dirent.h>
#include <string.h>

#include "fixture.h"
#include "tmpdir.h"

int fx_open_flags(fx *f, unsigned long flags)
{
    if (tmpdir_create(f->path, sizeof f->path) != 0)
    {
        return LEDGER89_EIO;
    }
    f->l = NULL;
    return ledger89_open(&f->l, f->path, flags);
}

int fx_open(fx *f)
{
    return fx_open_flags(f, LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE |
                                LEDGER89_OPEN_EXCL);
}

void fx_close(fx *f)
{
    if (f->l != NULL)
    {
        ledger89_close(f->l);
        f->l = NULL;
    }
}

int fx_reopen(fx *f)
{
    fx_close(f);
    return ledger89_open(&f->l, f->path,
                         LEDGER89_OPEN_RDWR | LEDGER89_OPEN_CREATE);
}

int fx_append(fx *f, const void *data, size_t size, ledger89_index *index_out)
{
    ledger89_slice s;

    s.data = data;
    s.size = size;
    return ledger89_appendv(f->l, &s, 1u, index_out);
}

int fx_read(fx *f, ledger89_index index, void *data, size_t capacity,
            size_t *size_out)
{
    return ledger89_read(f->l, index, data, capacity, size_out);
}

static int fx_count_prefix(const fx *f, const char *prefix)
{
    DIR *d;
    struct dirent *e;
    int n;
    size_t plen;

    plen = strlen(prefix);
    d = opendir(f->path);
    if (d == NULL)
    {
        return -1;
    }
    n = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strlen(e->d_name) == plen + 16u &&
            strncmp(e->d_name, prefix, plen) == 0)
        {
            ++n;
        }
    }
    closedir(d);
    return n;
}

int fx_count_parts(const fx *f)
{
    return fx_count_prefix(f, "part.");
}

int fx_count_manifests(const fx *f)
{
    return fx_count_prefix(f, "MANIFEST.");
}
