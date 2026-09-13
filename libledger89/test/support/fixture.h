#ifndef FIXTURE_H
#define FIXTURE_H

/* fixture.h - small black-box fixture for API tests: a fresh temp ledger
 * directory plus open/close helpers. Not part of the library. */

#include <stddef.h>

#include <ledger89.h>

typedef struct fx
{
    char path[64];
    ledger89_config config;
    ledger89 *l;
} fx;

/* Create a fresh directory, fill config, and open the ledger. */
int fx_open(fx *f);

/* Same, with explicit rotation targets (0 means the library default). */
int fx_open_limits(fx *f, unsigned long seg_bytes, unsigned long seg_records);

/* Close the handle if open. */
void fx_close(fx *f);

/* Close and reopen the same directory. */
int fx_reopen(fx *f);

/* Fill a record with the given fields. */
void fx_record(ledger89_record *r, ledger89_index index, unsigned long tag,
               const void *data, size_t size);

/* Append one record and return the status. */
int fx_append(fx *f, ledger89_index index, unsigned long tag, const void *data,
              size_t size);

/* Read one record into out and return the status. */
int fx_read(fx *f, ledger89_index index, ledger89_view *out);

/* Count sealed segment files in the fixture directory. */
int fx_count_sealed(const fx *f);

#endif /* FIXTURE_H */
