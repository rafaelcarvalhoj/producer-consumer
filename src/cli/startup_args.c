#include "cli/startup_args.h"
#include "cli/parsing/str_to_int.h"
#include <stdio.h>
#include <stdlib.h>

ProgramArguments *load_args(int argc, char **argv)
{
    if (argc != 3) {
        const char *program = argc > 0 && argv[0] != NULL
            ? argv[0] : "producer-consumer";
        fprintf(stderr, "Usage: %s <producers> <paradigm: 1|2>\n", program);
        return NULL;
    }

    int producers;
    if (!parse_int(argv[1], &producers) || producers <= 0) {
        fputs("Error: producers must be a positive integer.\n", stderr);
        return NULL;
    }

    int paradigm;
    if (!parse_int(argv[2], &paradigm) ||
        (paradigm != SHARED_MEMORY && paradigm != MESSAGE_EXCHANGE)) {
        fputs("Error: paradigm must be 1 (shared memory) or 2 (message exchange).\n", stderr);
        return NULL;
    }

    ProgramArguments *args = malloc(sizeof(*args));
    if (args == NULL) {
        perror("Failed to allocate arguments");
        return NULL;
    }

    args->producers = producers;
    args->paradigm = (ProducerConsumerParadigm)paradigm;

    return args;
}
