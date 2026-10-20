#include "cli/command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ProgramCommand *next_command(void)
{
    char prompt[3];
    char operation[9];
    if (scanf(" %2s %8s", prompt, operation) != 2) {
        return NULL;
    }
    if (strcmp(prompt, ">>") != 0) {
        return NULL;
    }

    ProgramCommand parsed = {0};
    if (strcmp(operation, "shutdown") == 0) {
        parsed.op = SHUTDOWN;
    } else {
        if (strcmp(operation, "produz") == 0) {
            parsed.op = PRODUCE;
        } else if (strcmp(operation, "consome") == 0) {
            parsed.op = CONSUME;
        } else {
            return NULL;
        }

        char extra[2];
        int ack;
        if (scanf(" %d %d %d %1s", &parsed.p, &parsed.n_msgs, &ack, extra) != 4 ||
            parsed.p <= 0 || parsed.n_msgs <= 0 ||
            (ack != YES && ack != NO) || strcmp(extra, "&") != 0) {
            return NULL;
        }
        parsed.ack = (Ack)ack;
    }

    ProgramCommand *command = malloc(sizeof(*command));
    if (command != NULL) {
        *command = parsed;
    }
    return command;
}

void free_command(ProgramCommand *command)
{
    free(command);
}

