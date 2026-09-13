/* llm89_sse.c - byte-oriented SSE field parser.
 *
 * Accepts LF, CRLF, and CR line endings. Only the `data` field affects event
 * delivery; comments and `event`/`id`/`retry`/unknown fields are ignored.
 * A line beginning `data:` strips one optional ASCII space after the colon.
 * Multiple `data:` lines in one event are concatenated with a single '\n'.
 * A blank line terminates the current event; an event with no `data:` lines
 * delivers nothing. */
#include <string.h>

#include "llm89_sse.h"

static void finish_event(llm_sse *p)
{
    if (p->has_data)
    {
        if (p->emit != NULL)
        {
            if (p->emit(p->userdata, llm_buf_data(&p->data),
                        llm_buf_len(&p->data)) != 0)
            {
                p->stopped = 1;
                return;
            }
        }
    }
    p->has_data = 0;
    llm_buf_reset(&p->data);
}

static void dispatch_line(llm_sse *p)
{
    const char *line;
    size_t len;
    size_t colon;
    size_t i;
    line = llm_buf_data(&p->line);
    len = llm_buf_len(&p->line);
    if (len == 0)
    {
        finish_event(p);
        llm_buf_reset(&p->line);
        return;
    }
    if (line[0] == ':')
    {
        llm_buf_reset(&p->line);
        return;
    }
    colon = (size_t)-1;
    i = 0;
    while (i < len)
    {
        if (line[i] == ':')
        {
            colon = i;
            break;
        }
        i = i + 1;
    }
    if (colon == (size_t)-1)
    {
        llm_buf_reset(&p->line);
        return;
    }
    if (colon == 4 && memcmp(line, "data", 4) == 0)
    {
        size_t vstart;
        size_t vlen;
        const char *v;
        vstart = colon + 1;
        vlen = len - vstart;
        v = line + vstart;
        if (vlen > 0 && v[0] == ' ')
        {
            v = v + 1;
            vlen = vlen - 1;
        }
        if (p->has_data)
        {
            if (llm_buf_append(&p->data, "\n", 1) != 0)
            {
                p->failed = 1;
                llm_buf_reset(&p->line);
                return;
            }
        }
        if (llm_buf_append(&p->data, v, vlen) != 0)
        {
            p->failed = 1;
            llm_buf_reset(&p->line);
            return;
        }
        p->has_data = 1;
    }
    llm_buf_reset(&p->line);
}

void llm_sse_init(llm_sse *p, size_t max_event_bytes, void *userdata,
                  llm_sse_emit_fn emit)
{
    llm_buf_init(&p->line, max_event_bytes);
    llm_buf_init(&p->data, max_event_bytes);
    p->has_data = 0;
    p->after_cr = 0;
    p->failed = 0;
    p->stopped = 0;
    p->userdata = userdata;
    p->emit = emit;
}

void llm_sse_destroy(llm_sse *p)
{
    llm_buf_destroy(&p->line);
    llm_buf_destroy(&p->data);
}

int llm_sse_feed(llm_sse *p, const char *bytes, size_t len)
{
    size_t i;
    i = 0;
    while (i < len)
    {
        char c;
        if (p->stopped)
        {
            return -1;
        }
        if (p->failed)
        {
            return 1;
        }
        c = bytes[i];
        if (p->after_cr)
        {
            p->after_cr = 0;
            if (c == '\n')
            {
                i = i + 1;
                continue;
            }
        }
        if (c == '\r')
        {
            dispatch_line(p);
            p->after_cr = 1;
            i = i + 1;
            continue;
        }
        if (c == '\n')
        {
            dispatch_line(p);
            i = i + 1;
            continue;
        }
        if (llm_buf_append(&p->line, &c, 1) != 0)
        {
            p->failed = 1;
            return 1;
        }
        i = i + 1;
    }
    if (p->failed)
    {
        return 1;
    }
    if (p->stopped)
    {
        return -1;
    }
    return 0;
}

int llm_sse_failed(const llm_sse *p)
{
    return p->failed;
}
