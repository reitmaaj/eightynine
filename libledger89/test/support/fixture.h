#ifndef FIXTURE_H
#define FIXTURE_H

/* fixture.h - small black-box fixture for API tests: a fresh temp ledger
 * directory plus open/close helpers. Not part of the library. */

#include <stddef.h>

#include <ledger89.h>

typedef struct fx
{
    char path[64];
    ledger89 *l;
} fx;

/* Create a fresh directory and open the ledger RDWR|CREATE|EXCL. */
int fx_open(fx *f);

/* Same, with explicit flags (the directory is still fresh). */
int fx_open_flags(fx *f, unsigned long flags);

/* Close the handle if open. */
void fx_close(fx *f);

/* Close and reopen the same directory with RDWR|CREATE. */
int fx_reopen(fx *f);

/* Append one record; index_out may be NULL. */
int fx_append(fx *f, const void *data, size_t size, ledger89_index *index_out);

/* Read one record into data; size_out receives the record size. */
int fx_read(fx *f, ledger89_index index, void *data, size_t capacity,
            size_t *size_out);

/* Count part files in the fixture directory. */
int fx_count_parts(const fx *f);

/* Count manifest generations in the fixture directory. */
int fx_count_manifests(const fx *f);

#endif /* FIXTURE_H */
