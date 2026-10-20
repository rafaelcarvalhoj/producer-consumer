#include "cli/parsing/str_to_int.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

bool parse_int(const char *text, int *result)
{
    char *end;

    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < INT_MIN || value > INT_MAX) {
        return false;
    }

    *result = (int)value;
    return true;
}

