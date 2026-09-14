/* strictrun.c - line-oriented scenario runner for the libledger89 strict
 * suite. Test-only; links the static library. Reads ops on stdin and prints
 * one transcript line per op on stdout.
 *
 * Exit status: 0 completed, 3 the fault shim fired, 1 protocol error. */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ledger89.h>

#define LINE_CAP (1u << 20)
#define PATH_CAP 4096u
#define HEX_CAP (LINE_CAP / 2u)

static char g_root[PATH_CAP];
static int g_have_root;

static unsigned long u64_to_ul(ledger89_u64 v)
{
    return ((unsigned long)v.hi << 32) | (unsigned long)v.lo;
}

static ledger89_u64 ul_to_u64(unsigned long v)
{
    ledger89_u64 out;

    out.hi = (ledger89_u32)(v >> 32);
    out.lo = (ledger89_u32)(v & 0xFFFFFFFFul);
    return out;
}

static int parse_ul(const char *text, unsigned long *out)
{
    char *end;
    unsigned long value;

    if (text == NULL)
    {
        return 0;
    }
    errno = 0;
    end = NULL;
    value = strtoul(text, &end, 0);
    if (errno != 0)
    {
        return 0;
    }
    if (end == text)
    {
        return 0;
    }
    if (*end != '\0')
    {
        return 0;
    }
    *out = value;
    return 1;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static size_t parse_hex(const char *text, unsigned char *out, size_t cap)
{
    size_t len;
    size_t i;
    size_t pos;

    len = strlen(text);
    if ((len % 2u) != 0u)
    {
        return (size_t)-1;
    }
    len /= 2u;
    if (len > cap)
    {
        return (size_t)-1;
    }
    pos = 0u;
    for (i = 0u; i < len; ++i)
    {
        int hi;
        int lo;

        hi = hex_value((unsigned char)text[pos]);
        lo = hex_value((unsigned char)text[pos + 1u]);
        if (hi < 0 || lo < 0)
        {
            return (size_t)-1;
        }
        out[i] = (unsigned char)((hi << 4) | lo);
        pos += 2u;
    }
    return len;
}

static unsigned long lcg_next(unsigned long *state)
{
    *state = (*state * 6364136223846793005ul) + 1442695040888963407ul;
    return *state;
}

static unsigned long crc32c(unsigned long crc, const unsigned char *data,
                            size_t len)
{
    size_t i;
    int bit;

    for (i = 0u; i < len; ++i)
    {
        crc ^= (unsigned long)data[i];
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc & 1ul) != 0ul)
            {
                crc = (crc >> 1) ^ 0x82F63B78ul;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc & 0xFFFFFFFFul;
}

static void print_hex(const unsigned char *data, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0u; i < len; ++i)
    {
        putchar(digits[(data[i] >> 4) & 0x0Fu]);
        putchar(digits[data[i] & 0x0Fu]);
    }
}

static int resolve_path(const char *in, char *out, size_t cap)
{
    int needed;

    if (in[0] == '/')
    {
        needed = (int)strlen(in);
        if ((size_t)needed + 1u > cap)
        {
            return 0;
        }
        strcpy(out, in);
        return 1;
    }
    if (!g_have_root)
    {
        return 0;
    }
    needed = (int)(strlen(g_root) + strlen(in) + 2u);
    if ((size_t)needed > cap)
    {
        return 0;
    }
    strcpy(out, g_root);
    strcat(out, "/");
    strcat(out, in);
    return 1;
}

static void strip_newline(char *line)
{
    size_t len;

    len = strlen(line);
    while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r'))
    {
        line[len - 1u] = '\0';
        --len;
    }
}

static int fault_fired(void)
{
    const char *path;
    struct stat st;

    path = getenv("LED89_FAULT_COUNT");
    if (path == NULL)
    {
        return 0;
    }
    if (stat(path, &st) != 0)
    {
        return 0;
    }
    if (st.st_size <= 0)
    {
        return 0;
    }
    return 1;
}

static int cmd_open(ledger89 **handle, char *args)
{
    char *flags_text;
    char *path_text;
    char path[PATH_CAP];
    unsigned long flags;
    int rc;

    flags_text = strtok(args, " \t");
    path_text = strtok(NULL, " \t");
    if (flags_text == NULL || path_text == NULL)
    {
        return 0;
    }
    if (!parse_ul(flags_text, &flags))
    {
        return 0;
    }
    if (!resolve_path(path_text, path, sizeof path))
    {
        return 0;
    }
    rc = ledger89_open(handle, path, flags);
    printf("rc=%d\n", rc);
    return 1;
}

static int cmd_append(ledger89 *handle, char *args)
{
    unsigned char *payload;
    size_t len;
    ledger89_index index;
    int rc;

    payload = (unsigned char *)malloc(HEX_CAP);
    if (payload == NULL)
    {
        return 0;
    }
    len = parse_hex(args, payload, HEX_CAP);
    if (len == (size_t)-1)
    {
        free(payload);
        return 0;
    }
    rc = ledger89_append(handle, len == 0u ? NULL : payload, len, &index);
    printf("rc=%d idx=%lu\n", rc, u64_to_ul(index));
    free(payload);
    return 1;
}

static int cmd_append_fill(ledger89 *handle, char *args)
{
    char *byte_text;
    char *len_text;
    unsigned long byte_value;
    unsigned long len_value;
    unsigned char *payload;
    ledger89_index index;
    int rc;

    byte_text = strtok(args, " \t");
    len_text = strtok(NULL, " \t");
    if (byte_text == NULL || len_text == NULL)
    {
        return 0;
    }
    if (!parse_ul(byte_text, &byte_value) || !parse_ul(len_text, &len_value))
    {
        return 0;
    }
    if (len_value > 16777216ul)
    {
        return 0;
    }
    payload = NULL;
    if (len_value > 0ul)
    {
        payload = (unsigned char *)malloc((size_t)len_value);
        if (payload == NULL)
        {
            return 0;
        }
        memset(payload, (int)(byte_value & 0xFFul), (size_t)len_value);
    }
    rc = ledger89_append(handle, payload, (size_t)len_value, &index);
    printf("rc=%d idx=%lu\n", rc, u64_to_ul(index));
    free(payload);
    return 1;
}

static int cmd_append_pattern(ledger89 *handle, char *args)
{
    char *seed_text;
    char *len_text;
    char *count_text;
    unsigned long seed;
    unsigned long len_value;
    unsigned long count;
    unsigned char *payload;
    ledger89_index first;
    size_t i;
    int rc;

    seed_text = strtok(args, " \t");
    len_text = strtok(NULL, " \t");
    count_text = strtok(NULL, " \t");
    if (seed_text == NULL || len_text == NULL || count_text == NULL)
    {
        return 0;
    }
    if (!parse_ul(seed_text, &seed) || !parse_ul(len_text, &len_value) ||
        !parse_ul(count_text, &count))
    {
        return 0;
    }
    if (len_value > 16777216ul || count > 4096ul)
    {
        return 0;
    }
    payload = (unsigned char *)malloc((size_t)len_value + 1u);
    if (payload == NULL)
    {
        return 0;
    }
    first = ul_to_u64(0ul);
    rc = LEDGER89_OK;
    for (i = 0ul; i < count; ++i)
    {
        unsigned long j;
        ledger89_index index;

        for (j = 0ul; j < len_value; ++j)
        {
            payload[j] = (unsigned char)(lcg_next(&seed) & 0xFFul);
        }
        rc = ledger89_append(handle, payload, (size_t)len_value, &index);
        if (rc != LEDGER89_OK)
        {
            break;
        }
        if (i == 0ul)
        {
            first = index;
        }
    }
    printf("rc=%d idx=%lu\n", rc, u64_to_ul(first));
    free(payload);
    return 1;
}

static int cmd_appendv(ledger89 *handle, char *args)
{
    ledger89_slice slices[4096];
    unsigned char *buffers[4096];
    size_t count;
    size_t i;
    ledger89_index first;
    char *save;
    char *token;
    int rc;

    count = 0u;
    save = NULL;
    first = ul_to_u64(0ul);
    token = strtok_r(args, ",", &save);
    while (token != NULL && count < 4096u)
    {
        size_t len;

        buffers[count] = (unsigned char *)malloc((strlen(token) / 2u) + 1u);
        if (buffers[count] == NULL)
        {
            break;
        }
        if (strcmp(token, "-") == 0)
        {
            len = 0u;
        }
        else
        {
            len = parse_hex(token, buffers[count], (strlen(token) / 2u) + 1u);
        }
        if (len == (size_t)-1)
        {
            break;
        }
        slices[count].data = len == 0u ? NULL : buffers[count];
        slices[count].size = len;
        ++count;
        token = strtok_r(NULL, ",", &save);
    }
    rc = LEDGER89_EINVAL;
    if (token == NULL && count > 0u)
    {
        rc = ledger89_appendv(handle, slices, count, &first);
    }
    printf("rc=%d idx=%lu\n", rc, u64_to_ul(first));
    for (i = 0u; i < count; ++i)
    {
        free(buffers[i]);
    }
    return 1;
}

static int cmd_appendv_at(ledger89 *handle, char *args)
{
    char *rev_text;
    char *end_text;
    char *list;
    unsigned long rev;
    unsigned long end;
    ledger89_slice slices[4096];
    unsigned char *buffers[4096];
    size_t count;
    size_t i;
    ledger89_index first;
    char *save;
    char *token;
    int rc;

    rev_text = strtok(args, " \t");
    end_text = strtok(NULL, " \t");
    list = strtok(NULL, "");
    if (rev_text == NULL || end_text == NULL || list == NULL)
    {
        return 0;
    }
    if (!parse_ul(rev_text, &rev) || !parse_ul(end_text, &end))
    {
        return 0;
    }
    count = 0u;
    save = NULL;
    first = ul_to_u64(0ul);
    token = strtok_r(list, ",", &save);
    while (token != NULL && count < 4096u)
    {
        size_t len;

        buffers[count] = (unsigned char *)malloc((strlen(token) / 2u) + 1u);
        if (buffers[count] == NULL)
        {
            break;
        }
        if (strcmp(token, "-") == 0)
        {
            len = 0u;
        }
        else
        {
            len = parse_hex(token, buffers[count], (strlen(token) / 2u) + 1u);
        }
        if (len == (size_t)-1)
        {
            break;
        }
        slices[count].data = len == 0u ? NULL : buffers[count];
        slices[count].size = len;
        ++count;
        token = strtok_r(NULL, ",", &save);
    }
    rc = LEDGER89_EINVAL;
    if (token == NULL && count > 0u)
    {
        rc = ledger89_appendv_at(handle, ul_to_u64(rev), ul_to_u64(end), slices,
                                 count, &first);
    }
    printf("rc=%d idx=%lu\n", rc, u64_to_ul(first));
    for (i = 0u; i < count; ++i)
    {
        free(buffers[i]);
    }
    return 1;
}

static int cmd_sync(ledger89 *handle)
{
    ledger89_index stable;
    int rc;

    rc = ledger89_sync(handle, &stable);
    printf("rc=%d stable=%lu\n", rc, u64_to_ul(stable));
    return 1;
}

static int cmd_read(ledger89 *handle, char *args)
{
    char *index_text;
    char *cap_text;
    unsigned long index;
    unsigned long cap;
    unsigned char *buffer;
    size_t size;
    int rc;

    index_text = strtok(args, " \t");
    cap_text = strtok(NULL, " \t");
    if (index_text == NULL || cap_text == NULL)
    {
        return 0;
    }
    if (!parse_ul(index_text, &index))
    {
        return 0;
    }
    size = 0u;
    if (strcmp(cap_text, "-") == 0)
    {
        rc = ledger89_read(handle, ul_to_u64(index), NULL, 0u, &size);
        printf("rc=%d size=%lu\n", rc, (unsigned long)size);
        return 1;
    }
    if (!parse_ul(cap_text, &cap))
    {
        return 0;
    }
    buffer = (unsigned char *)malloc((size_t)cap + 1u);
    if (buffer == NULL)
    {
        return 0;
    }
    memset(buffer, 0x5Au, (size_t)cap + 1u);
    rc = ledger89_read(handle, ul_to_u64(index), buffer, (size_t)cap, &size);
    if (rc == LEDGER89_OK)
    {
        printf("rc=%d size=%lu data=", rc, (unsigned long)size);
        print_hex(buffer, size);
        printf("\n");
    }
    else
    {
        printf("rc=%d size=%lu\n", rc, (unsigned long)size);
    }
    free(buffer);
    return 1;
}

static int cmd_read_crc(ledger89 *handle, char *args)
{
    char *index_text;
    unsigned long index;
    unsigned char *buffer;
    size_t size;
    size_t capacity;
    int rc;

    index_text = strtok(args, " \t");
    if (index_text == NULL)
    {
        return 0;
    }
    if (!parse_ul(index_text, &index))
    {
        return 0;
    }
    capacity = 16777216u;
    buffer = (unsigned char *)malloc(capacity);
    if (buffer == NULL)
    {
        return 0;
    }
    size = 0u;
    rc = ledger89_read(handle, ul_to_u64(index), buffer, capacity, &size);
    if (rc == LEDGER89_OK)
    {
        printf("rc=%d size=%lu crc=%08lx\n", rc, (unsigned long)size,
               crc32c(0ul, buffer, size));
    }
    else
    {
        printf("rc=%d size=%lu\n", rc, (unsigned long)size);
    }
    free(buffer);
    return 1;
}

static int cmd_iter_init(ledger89_iter *iter, ledger89 *handle, char *args)
{
    unsigned long from;
    int rc;

    if (!parse_ul(args, &from))
    {
        return 0;
    }
    rc = ledger89_iter_init(iter, handle, ul_to_u64(from));
    printf("rc=%d\n", rc);
    return 1;
}

static int cmd_iter_next(ledger89_iter *iter, char *args)
{
    unsigned long cap;
    unsigned char *buffer;
    ledger89_index index;
    size_t size;
    int rc;

    if (!parse_ul(args, &cap))
    {
        return 0;
    }
    buffer = (unsigned char *)malloc((size_t)cap + 1u);
    if (buffer == NULL)
    {
        return 0;
    }
    size = 0u;
    rc = ledger89_iter_next(iter, &index, (size_t)cap == 0ul ? NULL : buffer,
                            (size_t)cap, &size);
    if (rc == LEDGER89_OK)
    {
        printf("rc=%d idx=%lu size=%lu data=", rc, u64_to_ul(index),
               (unsigned long)size);
        print_hex(buffer, size);
        printf("\n");
    }
    else
    {
        printf("rc=%d idx=%lu size=%lu\n", rc, u64_to_ul(index),
               (unsigned long)size);
    }
    free(buffer);
    return 1;
}

static int cmd_truncate(ledger89 *handle, char *args)
{
    unsigned long from;
    int rc;

    if (!parse_ul(args, &from))
    {
        return 0;
    }
    rc = ledger89_truncate_from(handle, ul_to_u64(from));
    printf("rc=%d\n", rc);
    return 1;
}

static int cmd_prune(ledger89 *handle, char *args)
{
    unsigned long requested;
    ledger89_index actual;
    int rc;

    if (!parse_ul(args, &requested))
    {
        return 0;
    }
    rc = ledger89_prune_before(handle, ul_to_u64(requested), &actual);
    printf("rc=%d actual=%lu\n", rc, u64_to_ul(actual));
    return 1;
}

static int cmd_rotate(ledger89 *handle)
{
    int rc;

    rc = ledger89_rotate(handle);
    printf("rc=%d\n", rc);
    return 1;
}

static int cmd_verify(ledger89 *handle)
{
    int rc;

    rc = ledger89_verify(handle);
    printf("rc=%d\n", rc);
    return 1;
}

static int cmd_state(ledger89 *handle)
{
    ledger89_state state;
    size_t i;
    int rc;

    rc = ledger89_get_state(handle, &state);
    if (rc != LEDGER89_OK)
    {
        printf("rc=%d\n", rc);
        return 1;
    }
    printf("rc=0 first=%lu stable=%lu end=%lu rev=%lu id=",
           u64_to_ul(state.first), u64_to_ul(state.stable_end),
           u64_to_ul(state.end), u64_to_ul(state.revision));
    for (i = 0u; i < 16u; ++i)
    {
        printf("%02x", (unsigned)state.id.bytes[i]);
    }
    printf("\n");
    return 1;
}

static int dispatch(ledger89 **handle, ledger89_iter *iter, char *line)
{
    char *cmd;
    char *args;

    cmd = strtok(line, " \t");
    if (cmd == NULL)
    {
        return 1;
    }
    args = strtok(NULL, "");
    if (strcmp(cmd, "open") == 0)
    {
        return cmd_open(handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "close") == 0)
    {
        ledger89_close(*handle);
        *handle = NULL;
        printf("rc=0\n");
        return 1;
    }
    if (strcmp(cmd, "state") == 0)
    {
        return cmd_state(*handle);
    }
    if (strcmp(cmd, "append") == 0)
    {
        return cmd_append(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "append_fill") == 0)
    {
        return cmd_append_fill(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "append_pattern") == 0)
    {
        return cmd_append_pattern(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "appendv") == 0)
    {
        return cmd_appendv(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "appendv_at") == 0)
    {
        return cmd_appendv_at(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "sync") == 0)
    {
        return cmd_sync(*handle);
    }
    if (strcmp(cmd, "read") == 0)
    {
        return cmd_read(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "read_crc") == 0)
    {
        return cmd_read_crc(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "iter_init") == 0)
    {
        return cmd_iter_init(iter, *handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "iter_next") == 0)
    {
        return cmd_iter_next(iter, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "truncate") == 0)
    {
        return cmd_truncate(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "prune") == 0)
    {
        return cmd_prune(*handle, args == NULL ? (char *)"" : args);
    }
    if (strcmp(cmd, "rotate") == 0)
    {
        return cmd_rotate(*handle);
    }
    if (strcmp(cmd, "verify") == 0)
    {
        return cmd_verify(*handle);
    }
    if (strcmp(cmd, "nop") == 0)
    {
        printf("rc=0\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    static char line[LINE_CAP];
    ledger89 *handle;
    ledger89_iter iter;
    const char *root;
    int status;

    signal(SIGXFSZ, SIG_IGN);
    root = getenv("LED89_STRICT_ROOT");
    g_have_root = 0;
    if (root != NULL && root[0] != '\0')
    {
        if (strlen(root) + 1u > sizeof g_root)
        {
            return 1;
        }
        strcpy(g_root, root);
        g_have_root = 1;
    }
    handle = NULL;
    memset(&iter, 0, sizeof iter);
    status = 0;
    while (fgets(line, (int)sizeof line, stdin) != NULL)
    {
        strip_newline(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }
        if (!dispatch(&handle, &iter, line))
        {
            fprintf(stderr, "strictrun: bad op: %s\n", line);
            ledger89_close(handle);
            fflush(stdout);
            return 1;
        }
    }
    if (handle != NULL)
    {
        ledger89_close(handle);
    }
    fflush(stdout);
    if (fault_fired())
    {
        status = 3;
    }
    return status;
}
