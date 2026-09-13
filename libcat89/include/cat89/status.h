#ifndef CAT89_STATUS_H
#define CAT89_STATUS_H

/* cat89_status.h - single status domain for libcat89. */

typedef enum cat89_status
{
    CAT89_OK = 0,
    CAT89_INVALID,
    CAT89_DOMAIN,
    CAT89_NOT_SUPPORTED,
    CAT89_NOT_FOUND,
    CAT89_NOMEM,
    CAT89_CALLBACK
} cat89_status;

/* Return a stable, non-null textual name for a defined status value. */
const char *cat89_status_name(cat89_status status);

#endif
