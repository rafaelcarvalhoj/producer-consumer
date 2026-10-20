#define _POSIX_C_SOURCE 200809L

#include "cli/command_parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

void test_next_command(void)
{
    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs(">> produz 2 5 2 &\n"
                 ">> consome 3 4 1 &\n"
                 ">> shutdown\n"
                 ">> produz 2 5 3 &\n"
                 ">> desconhecido\n", input) >= 0);
    rewind(input);

    int original_stdin = dup(STDIN_FILENO);
    assert(original_stdin >= 0);
    assert(dup2(fileno(input), STDIN_FILENO) == STDIN_FILENO);
    clearerr(stdin);

    ProgramCommand *command = next_command();
    assert(command != NULL);
    assert(command->op == PRODUCE);
    assert(command->p == 2);
    assert(command->n_msgs == 5);
    assert(command->ack == NO);
    free_command(command);

    command = next_command();
    assert(command != NULL);
    assert(command->op == CONSUME);
    assert(command->p == 3);
    assert(command->n_msgs == 4);
    assert(command->ack == YES);
    free_command(command);

    command = next_command();
    assert(command != NULL);
    assert(command->op == SHUTDOWN);
    free_command(command);

    assert(next_command() == NULL);
    assert(next_command() == NULL);

    assert(dup2(original_stdin, STDIN_FILENO) == STDIN_FILENO);
    assert(close(original_stdin) == 0);
    assert(fclose(input) == 0);
    clearerr(stdin);
}
