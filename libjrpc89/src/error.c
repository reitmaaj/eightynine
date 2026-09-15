/* error.c - JSON-RPC 2.0 error-code classification. */
#include <jrpc89.h>

int jrpc89_error_code_reserved(j89_int code)
{
    int r;
    r = 0;
    if (code >= -32768.0)
    {
        if (code <= -32000.0)
        {
            r = 1;
        }
    }
    return r;
}
