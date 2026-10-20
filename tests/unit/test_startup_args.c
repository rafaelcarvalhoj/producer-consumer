#include "cli/startup_args.h"

#include <assert.h>
#include <stdlib.h>

static void expect_invalid(int argc, char **argv)
{
    ProgramArguments *args = load_args(argc, argv);
    assert(args == NULL);
}

void test_startup_args(void)
{
    char *shared[] = {"program", "3", "1", NULL};
    ProgramArguments *args = load_args(3, shared);
    assert(args != NULL);
    assert(args->producers == 3);
    assert(args->paradigm == SHARED_MEMORY);
    free(args);

    char *messages[] = {"program", "1", "2", NULL};
    args = load_args(3, messages);
    assert(args != NULL);
    assert(args->producers == 1);
    assert(args->paradigm == MESSAGE_EXCHANGE);
    free(args);

    expect_invalid(1, shared);
    expect_invalid(2, shared);
    char *extra[] = {"program", "3", "1", "extra", NULL};
    expect_invalid(4, extra);

    char *zero[] = {"program", "0", "1", NULL};
    char *negative[] = {"program", "-1", "1", NULL};
    char *non_numeric[] = {"program", "three", "1", NULL};
    expect_invalid(3, zero);
    expect_invalid(3, negative);
    expect_invalid(3, non_numeric);

    char *invalid_paradigm[] = {"program", "3", "3", NULL};
    char *text_paradigm[] = {"program", "3", "shared", NULL};
    expect_invalid(3, invalid_paradigm);
    expect_invalid(3, text_paradigm);
}
