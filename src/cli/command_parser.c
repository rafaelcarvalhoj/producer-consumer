#include "cli/command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ProgramCommand *next_command(void)
{
    char operation[9];
    ProgramCommand parsed = {0};
    int ack;
    if (scanf(">>%8s %d %d %d", operation, &parsed.p, &parsed.n_msgs,&ack) != 4)
    {
        return NULL;
    }
    while ((getchar()) != '\n');
    if(ack==1){
        parsed.ack = YES;
    }
    else{
        parsed.ack = NO;
    }
    printf("esse aqui foi o comando %s %d %d %d\n", operation, parsed.p, parsed.n_msgs, parsed.ack);
    if (strcmp(operation, "shutdown") == 0)
    {
        parsed.op = SHUTDOWN;
    }
    else
    {
        if (strcmp(operation, "produz") == 0)
        {
            parsed.op = PRODUCE;
        }
        else if (strcmp(operation, "consome") == 0)
        {
            parsed.op = CONSUME;
        }
        else
        {
            return NULL;
        }
    }

    ProgramCommand *command = malloc(sizeof(*command));
    command->op = parsed.op;
    command->p = parsed.p;
    command->n_msgs = parsed.n_msgs;
    command->ack = parsed.ack;

    return command;
}

void free_command(ProgramCommand *command)
{
    free(command);
}
