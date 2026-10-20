#include <stdio.h>
#include <stdlib.h>
#include "cli/startup_args.h"
#include "unions/paradigm.h"

static void startup_log(const ProgramArguments *args)
{
    puts("Producer-Consumer");
    printf("Producers: %d\n", args->producers);
    printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

int main(int argc, char **argv)
{
    ProgramArguments *args = load_args(argc, argv);
    if (args == NULL) {
        return EXIT_FAILURE;
    }

    startup_log(args);
    free(args);
    return EXIT_SUCCESS;
}
