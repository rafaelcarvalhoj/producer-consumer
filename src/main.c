#include <stdio.h>
#include <stdlib.h>
#include "cli/startup_args.h"
#include "cli/command_parser.h"


static void startup_log(const ProgramArguments *args) {
    puts("Producer-Consumer");
    printf("Producers: %d\n", args->producers);
    printf("Paradigm: %s\n", paradigm_to_str(args->paradigm));
}

int main(int argc, char **argv) {
    ProgramArguments *args = load_args(argc, argv);
    if (args == NULL) {
        return EXIT_FAILURE;
    }

    startup_log(args);

    ProgramCommand *cmd;

    for (;;) {
        cmd = next_command();
        if (cmd == NULL) {
            free(args);
            return EXIT_FAILURE;
        }

        CommandOperation operation = cmd->op;
        free_command(cmd);
        if (operation == SHUTDOWN) {
            break;
        }
    }
    free(args);
    return EXIT_SUCCESS;
}
