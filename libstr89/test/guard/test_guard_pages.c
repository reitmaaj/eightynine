/* test_guard_pages.c - no read may pass the reported length.
 *
 * Input bytes are placed so that the final byte abuts an inaccessible page;
 * any strlen-style scan or off-by-one read faults immediately. This test is
 * POSIX-specific and runs outside the green source gate.
 */

#define _GNU_SOURCE

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "str89_test.h"

static void exercise(const unsigned char *data, size_t len, const char *what)
{
    str89_view v;
    str89 s;
    str89_buf b;
    size_t at;
    int r;

    r = str89_view_init(&v, data, len);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_view(v, what);
    str89_test_check(str89_view_equal(v, v) != 0, what);
    str89_test_check(str89_view_compare(v, v) == 0, what);
    r = str89_view_find(v, v, 0, &at);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_check(at == 0, what);

    str89_init(&s);
    r = str89_from_view(&s, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_free(&s, NULL);

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    r = str89_buf_append(&b, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_check(b.len == len * 2, what);
    str89_buf_free(&b, NULL);
}

int main(void)
{
    long page;
    unsigned char *base;
    unsigned char *mid;
    unsigned char *data;
    int r;

    page = sysconf(_SC_PAGESIZE);
    if (page <= 0)
    {
        str89_test_check(0, "guard: page size");
        return str89_test_report();
    }
    base = (unsigned char *)mmap(NULL, (size_t)page * 3, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
    {
        str89_test_check(0, "guard: mmap");
        return str89_test_report();
    }
    mid = base + page;
    r = mprotect(base, (size_t)page, PROT_NONE);
    str89_test_check(r == 0, "guard: lower mprotect");
    r = mprotect(base + (size_t)page * 2, (size_t)page, PROT_NONE);
    str89_test_check(r == 0, "guard: upper mprotect");

    /* ASCII + 2-byte + 3-byte ending at the last accessible byte. */
    data = base + (size_t)page * 2 - 6;
    data[0] = 0x41;
    data[1] = 0xC3;
    data[2] = 0xA9;
    data[3] = 0xE2;
    data[4] = 0x82;
    data[5] = 0xAC;
    exercise(data, 6, "guard: mixed ends at page edge");

    /* One 4-byte scalar ending at the last accessible byte. */
    data = base + (size_t)page * 2 - 4;
    data[0] = 0xF0;
    data[1] = 0x90;
    data[2] = 0x8D;
    data[3] = 0x88;
    exercise(data, 4, "guard: 4-byte ends at page edge");

    /* Bytes starting at the first accessible byte. */
    data = mid;
    data[0] = 0x41;
    data[1] = 0xC3;
    data[2] = 0xA9;
    exercise(data, 3, "guard: starts at page start");

    /* Empty view at the guard boundary reads nothing. */
    {
        str89_view v;

        r = str89_view_init(&v, base + (size_t)page * 2, 0);
        str89_test_check_status(r, STR89_OK, "guard: empty at edge");
        r = str89_view_is_boundary(v, 0);
        str89_test_check(r == 1, "guard: empty boundary");
    }

    munmap(base, (size_t)page * 3);
    return str89_test_report();
}
