#include "cli/parsing/str_to_int.h"

#include <assert.h>
#include <limits.h>

void test_str_to_int(void)
{
    int value = 0;

    assert(parse_int("0", &value) && value == 0);
    assert(parse_int("42", &value) && value == 42);
    assert(parse_int("+42", &value) && value == 42);
    assert(parse_int("-42", &value) && value == -42);

    assert(parse_int("2147483647", &value) && value == INT_MAX);
    assert(parse_int("-2147483648", &value) && value == INT_MIN);

    assert(!parse_int("", &value));
    assert(!parse_int("abc", &value));
    assert(!parse_int("12abc", &value));
    assert(!parse_int("2.5", &value));
    assert(!parse_int("2147483648", &value));
    assert(!parse_int("-2147483649", &value));
}
