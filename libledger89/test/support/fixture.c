/* fixture.c - black-box fixture for API tests. */

#include <dirent.h>
#include <string.h>

#include "fixture.h"
#include "tmpdir.h"

int fx_open_limits(fx *f, unsigned long seg_bytes, unsigned long seg_records)
{
    int rc;

    if (tmpdir_create(f->path, sizeof f->path) != 0)
    {
        return LEDGER89_ERR_IO;
    }
    memset(&f->config, 0, sizeof f->config);
    f->config.path = f->path;
    f->config.max_segment_bytes = seg_bytes;
    f->config.max_segment_records = seg_records;
    f->l = NULL;
    rc = ledger89_open(&f->l, &f->config);
    return rc;
}

int fx_open(fx *f)
{
    return fx_open_limits(f, 1048576ul, 0ul);
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
    return ledger89_open(&f->l, &f->config);
}

void fx_record(ledger89_record *r, ledger89_index index, unsigned long tag,
               const void *data, size_t size)
{
    r->index = index;
    r->tag = tag;
    r->data = data;
    r->size = size;
}

int fx_append(fx *f, ledger89_index index, unsigned long tag, const void *data,
              size_t size)
{
    ledger89_record r;

    fx_record(&r, index, tag, data, size);
    return ledger89_append(f->l, &r, 1u);
}

int fx_read(fx *f, ledger89_index index, ledger89_view *out)
{
    return ledger89_read(f->l, index, out);
}

int fx_count_sealed(const fx *f)
{
    DIR *d;
    struct dirent *e;
    int n;

    d = opendir(f->path);
    if (d == NULL)
    {
        return -1;
    }
    n = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strlen(e->d_name) == 24u)
        {
            if (strcmp(e->d_name + 20, ".seg") == 0)
            {
                ++n;
            }
        }
    }
    closedir(d);
    return n;
}
