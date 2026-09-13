/* cat89_status.c - single status domain. */

#include <cat89/status.h>

const char *cat89_status_name(cat89_status status)
{
    switch (status)
    {
    case CAT89_OK:
        return "ok";
    case CAT89_INVALID:
        return "invalid";
    case CAT89_DOMAIN:
        return "domain";
    case CAT89_NOT_SUPPORTED:
        return "not_supported";
    case CAT89_NOT_FOUND:
        return "not_found";
    case CAT89_NOMEM:
        return "nomem";
    case CAT89_CALLBACK:
        return "callback";
    default:
        return "unknown";
    }
}
