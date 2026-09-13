/* header_check.cpp - the public header compiles under C++23 and keeps its
 * C ABI shape. */

#include <cstddef>

#include "str89.h"

static_assert(sizeof(str89_view) == sizeof(void *) + sizeof(std::size_t),
              "str89_view layout");
static_assert(sizeof(str89) == sizeof(void *) + sizeof(std::size_t),
              "str89 layout");
static_assert(sizeof(str89_buf) ==
                  sizeof(void *) + 2 * sizeof(std::size_t),
              "str89_buf layout");

int main()
{
    str89 s;

    str89_init(&s);
    if (s.data == nullptr)
    {
        if (s.len == 0)
        {
            return 0;
        }
    }
    return 1;
}
